void __fastcall TForm1::IdUDPServer1UDPRead(TIdUDPListenerThread *AThread, const TIdBytes AData, TIdSocketHandle *ABinding) {
    const auto now = std::chrono::system_clock::now();
#ifdef UNICODE
    using tok_t = std::string;
#else
    using tok_t = String;
#endif
    auto toStr = [&](const std::string &s) { return String(s.c_str()); };
    auto tokensFromBytes = [&](const TIdBytes &bytes, char sep) {
        std::vector<tok_t> tok;
        tok.emplace_back();
        int seps = 0;
        for (int i = 0; i < bytes.Length; ++i) {
            if (bytes[i] == sep) {
                ++seps;
                if (i == 0 || i == bytes.Length - 1)
                    continue;
                tok.emplace_back();
            } else {
#ifdef UNICODE
                tok.back().push_back(static_cast<char>(bytes[i]));
#else
                tok.back() += static_cast<char>(bytes[i]);
#endif
            }
        }
        return std::pair{tok, seps};
    };
    auto [tok, sepCount] = tokensFromBytes(AData, separator);
    if (Inifile->ReadBool("CONFIG", "SWAP_CODE_SPSCODE", 0) && tok.size() > 3)
        std::swap(tok[2], tok[3]);
	if (!sepCount) {
		DspLogAdd("Kein Separator gefunden!" + BytesToString(AData, 0, 90), __LINE__, clRed);
		StatusBar1->Panels->Items[2]->Text = "Keine Separatoren im Empfangsstring erkannt!";
		return;
	}
	const String host = Inifile->ReadString("CONFIG", "SPSIP", "localhost");
	const String tmpDat(BytesToString(AData, 0, 1000));
	std::string s = AnsiString(tmpDat).c_str();
	auto fdExecOpen = [&](TFDQuery *q) {
		try {
			q->Open();
			return true;
		} catch (EIBNativeException &e) {
			DspLogAdd(e.Message, __LINE__, clRed);
		} catch (EFDException &e) {
			DspLogAdd(e.Message, __LINE__, clRed);
		}
		return false;
	};
	auto fdExecSQL = [&](TFDQuery *q) {
		try {
			q->ExecSQL();
			q->Close();
			return true;
		} catch (EIBNativeException &e) {
			DspLogAdd(e.Message, __LINE__, clRed);
		} catch (EFDException &e) {
			DspLogAdd(e.Message, __LINE__, clRed);
		}
		return false;
	};
	auto idBytesToHex = [&](const TBytes &idBytes) {
		std::ostringstream oss;
		for (int i = 0; i < idBytes.Length; ++i)
			oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(idBytes[i]);
		return oss.str();
	};
	if (tok.empty() || tok[0].empty()) {
		DspLogAdd("Sendier-ID Ungültig!", __LINE__, clRed);
		return;
	}
	if (tok[0] == "LOG") {
		std::ofstream out(tok[1], std::ios::app);
		out << tok[2] << std::endl;
		std::stringstream ss;
		for (auto &x : tok)
			ss << x << separator;
		SENDUDP(String(ss.str().c_str()));
		return;
    }
    std::string default_table = "DATEN";
    if (tok.size() > 3 && tok[2] == "0033") {
        std::ostringstream sql;
        sql << "INSERT INTO " << default_table << " (DBK,CODE,SPSK) VALUES('" << tok[0] << "','" << tok[2] << "','" << tok[3] << "') returning id {into :new_id}";
        FDQuery1->SQL->Text = sql.str().c_str();
        try {
            FDQuery1->Params->CreateParam(ftByte, "new_id", ptOutput)->DataType = ftBytes;
            if (!fdExecOpen(FDQuery1))
                return;
            auto hex = idBytesToHex(FDQuery1->FieldByName("id")->AsBytes);
            FDQuery1->Close();
            std::ostringstream antwort;
            antwort << tok[0] << separator << tok[1] << separator << tok[2] << separator << tok[3] << separator << hex << separator;
			DspLogAdd("Neue ID Angefordert.", __LINE__, clLime);
			SENDUDP(antwort.str().c_str());
		} catch (EFDException &e) {
            DspLogAdd(e.Message, __LINE__, clRed);
        }
        return;
    }
    if (tok.size() >= 5 && (tok[2] == "0120" || tok[2] == "0121" || tok[2] == "0131" || tok[2] == "0132" || tok[2] == "0123" || tok[2] == "0133")) {
        default_table = tok[4];
        tok.erase(tok.begin() + 4);
    } else if (tok.size() < 5 && (tok[2] == "0120" || tok[2] == "0121" || tok[2] == "0131" || tok[2] == "0132" || tok[2] == "0123" || tok[2] == "0133")) {
        DspLogAdd("Keine Daten empfangen! " + BytesToString(AData, 0, 90), __LINE__, clRed);
        return;
    }
    if (tok.size() >= 6 && tok[2] == "0041") {
        static String m_lfdn{String(tok[4].c_str())};
        static String KurvenTyp{String(tok[5].c_str())};
        default_table = "IMAGES";
        std::ostringstream ss;
        ss << "INSERT INTO " << default_table << " (LFDN, SUBNR ,KURVENTYP , SN, TUPELSIZE, KOMMASTELLEN, SPALTEN, PAYLOADLENGTH, PAYLOAD) VALUES (:LFDN, :SUBNR, :KURVENTYP, :SN, :TUPELSIZE, :KOMMASTELLEN, :SPALTEN, :PAYLOADLENGTH, :PAYLOAD)";
        FDQuery1->SQL->Text = ss.str().c_str();
        FDQuery1->Params->CreateParam(ftString, "LFDN", ptInput);
        FDQuery1->ParamByName("LFDN")->AsString = String(tok[1].c_str());
        FDQuery1->Params->CreateParam(ftInteger, "SUBNR", ptInput);
        FDQuery1->ParamByName("SUBNR")->AsInteger = tok[4][0];
        FDQuery1->Params->CreateParam(ftInteger, "PAYLOADLENGTH", ptInput);
        FDQuery1->ParamByName("PAYLOADLENGTH")->AsInteger = AData.Length;
        if (tok[4][0] == 0) {
            FDQuery1->Params->CreateParam(ftString, "SN", ptInput);
            FDQuery1->ParamByName("SN")->AsString = String(tok[6].c_str());
            FDQuery1->Params->CreateParam(ftString, "KURVENTYP", ptInput);
            FDQuery1->ParamByName("KURVENTYP")->AsString = String(tok[5].c_str());
            FDQuery1->Params->CreateParam(ftInteger, "TUPELSIZE", ptInput);
            FDQuery1->ParamByName("TUPELSIZE")->AsInteger = tok[7][0];
            FDQuery1->Params->CreateParam(ftString, "KOMMASTELLEN", ptInput);
            FDQuery1->ParamByName("KOMMASTELLEN")->AsString = String(tok[8].c_str());
            FDQuery1->Params->CreateParam(ftString, "SPALTEN", ptInput);
            FDQuery1->ParamByName("SPALTEN")->AsString = String(tok[9].c_str());
        }
        std::unique_ptr<TMemoryStream> ms(new TMemoryStream());
        WriteTIdBytesToStream(ms.get(), AData, AData.Length, 0);
        FDQuery1->Params->CreateParam(ftBytes, "PAYLOAD", ptInput);
        FDQuery1->ParamByName("PAYLOAD")->LoadFromStream(ms.get(), ftBytes);
        if (fdExecSQL(FDQuery1)) {
            DspLogAdd(String().sprintf(L"Messdaten empfangen. %d Bytes.", AData.Length), __LINE__, clLime);
            IdUDPServer1->SendBuffer(host, Inifile->ReadInteger("CONFIG", "PORT", UDPPORT), AData);
        }
        return;
    }
	if (tok.size() > 1 && tok[0] == "PRINT") {
		if (!FileExists(GetCurrentDir() + "\\udcprint.exe")) {
            DspLogAdd("Druckfunktion nicht möglich! UDCPRINT fehlt!", __LINE__, clRed);
            return;
        }
        if (!DirectoryExists("Spool"))
            if (!CreateDir("Spool"))
                throw Exception("Cannot create Spool directory.");
        String filename = "Spool\\" + FormatDateTime(L"yymmddhhmmss_z", Now());
        {
            std::ofstream out(AnsiString(filename).c_str(), std::ios::out);
            out << "[PRINTER]\n";
            out << "SN=" << tok[1] << "\n";
            out << "PRINTFILE=" << tok[2] << "\n";
            std::string all;
            for (const auto &p : tok) {
                all += p;
                all += separator;
            }
            out << "UDPDruckAuftrag=" << all << "\n";
        }
        if (!no_printspooler) {
            auto processId = FindProcessId(L"udcprint.exe");
#ifdef UNICODE
            if (!processId)
                RunApp(NULL, GetCurrentDir() + "\\udcprint.exe");
#else
            if (!processId)
                RunApp(tmp_udc_ini);
#endif
        }
        std::string all;
        for (const auto &p : tok) {
            all += p;
            all += separator;
        }
		SENDUDP(all.c_str());
		return;
    }
    static int entries = 1;
    static int ErrorStatus = 1;
    std::deque<fldstruct> fld;
    int code = 0;
    {
        int i = 0, j = 0;
        std::map<std::string, int> dupfld;
        fldstruct tmpfld;
        for (auto t : tok) {
            ++i;
            if (i < 5) {
                if (i == 1)
                    tmpfld.name = "DBK";
                if (i == 2)
                    tmpfld.name = "SN";
                if (i == 3) {
                    tmpfld.name = "CODE";
                    if (t == "") {
                        DspLogAdd("Code nicht angegeben!", __LINE__, clRed);
                        return;
                    }
                    code = std::stoi(t);
                }
                if (i == 4)
                    tmpfld.name = "SPSK";
                tmpfld.value = trim3(t);
                fld.push_back(tmpfld);
                continue;
            }
            ++j;
            switch (code) {
            case 20:
            case 120:
            case 121:
            case 21:
            case 22:
            case 23:
            case 123: {
                if (!(j % 3)) {
                    std::transform(t.begin(), t.end(), t.begin(), ::toupper);
                    dupfld[t]++;
                    if (dupfld[t] > 1) {
                        StatusBar1->Panels->Items[2]->Text = "Feld " + String(t.c_str()) + " existiert bereits";
                        ErrorStatus = 2;
                        return;
                    }
                    if (reserved_word.find(t) != reserved_word.end()) {
                        StatusBar1->Panels->Items[2]->Text = "Unzulässiger Feldname: " + String(t.c_str());
                        ErrorStatus = 3;
                        return;
                    }
                    if (t.empty()) {
                        StatusBar1->Panels->Items[2]->Text = "Unzulässiger Feldname: ";
                        ErrorStatus = 4;
                        return;
                    }
                    tmpfld.name = t.c_str();
                    fld.push_back(tmpfld);
                }
                if (!((j - 1) % 3)) {
                    if (t.empty()) {
                        StatusBar1->Panels->Items[2]->Text = "Keine Felddaten angegeben!";
                        return;
                    }
                    tmpfld.value = trim3(t);
                }
                if (!((j - 2) % 3)) {
                    for (auto &c : t)
                        c = std::tolower(c);
                    if (t.empty()) {
                        StatusBar1->Panels->Items[2]->Text = "Datentyp nicht angegeben!";
                        return;
                    }
                    tmpfld.type = trim3(t);
                }
            } break;
            case 31:
            case 131:
            case 132: {
                if (!(j % 2)) {
                    for (auto &c : t)
                        c = std::toupper(c);
                    tmpfld.name = t.c_str();
                    fld.push_back(tmpfld);
                }
                if (!((j - 1) % 2)) {
                    tmpfld.type = t.c_str();
                    tmpfld.value = "0";
                }
            } break;
            case 50:
            case 51:
            case 52: {
                tmpfld.value = t.c_str();
                tmpfld.name = "";
                tmpfld.type = "";
                fld.push_back(tmpfld);
            } break;
            case 99:
                break;
            default:
                DspLogAdd("Code " + String(code) + " nicht definiert!", __LINE__, clRed);
                return;
            }
        }
    }
#ifndef _DEBUG
    if (code != 99)
        DspLogAdd(String().sprintf(L"%s %s:%d %s:%d %s", Now().TimeString().c_str(), ABinding->PeerIP.c_str(), ABinding->PeerPort, ABinding->IP.c_str(), ABinding->Port, tmpDat.c_str()));
#endif
    if (code == 99) {
        Shape2->Brush->Color = clLime;
        std::stringstream ss;
        auto t = std::time(nullptr);
        auto *b = std::localtime(&t);
        ss << s << std::setw(2) << std::setfill('0') << b->tm_mday
           << std::setw(2) << b->tm_mon + 1
           << std::setw(2) << (b->tm_year + 1900) - 2000
           << separator
           << std::setw(2) << b->tm_hour
           << std::setw(2) << b->tm_min
           << std::setw(2) << b->tm_sec
           << separator
           << std::setw(2) << ErrorStatus;
        SENDUDP(ss.str().c_str());
        hbstart = start = std::chrono::system_clock::now();
        return;
    }
    if (tok[RX_CODE][1] == '2') {
        UnicodeString d = AnsiString(Now().FormatString("yymmdd")).c_str();
        if (code % 2)
            Inifile->WriteInteger("ZAEHLER", String(code - 1), 0);
        else {
            if (Inifile->ReadString("ZAEHLER", "Date_" + String(code), "160101") != d)
                Inifile->WriteInteger("ZAEHLER", String(code), 0);
            Inifile->WriteString("ZAEHLER", "Date_" + String(code), d);
            Inifile->WriteInteger("ZAEHLER", String(code), Inifile->ReadInteger("ZAEHLER", String(code), 0) + 1);
        }
        std::stringstream ss;
        ss << fld[1].value.substr(0, 10) << AnsiString(d).c_str() << std::setw(4) << std::setfill('0') << AnsiString(Inifile->ReadString("ZAEHLER", String(code), 0)).c_str();
        fld[1].value = ss.str();
        ss.str(std::string());
        for (auto &a : fld)
            ss << a.value << separator;
        s = ss.str();
        DspLogAdd(String().sprintf(L" %s %s:%d %s:%d %S", Now().TimeString().c_str(), ABinding->IP.c_str(), ABinding->Port, ABinding->PeerIP.c_str(), ABinding->PeerPort, s.c_str()));
        SENDUDP(s.c_str());
    }
    if (code == 30 || code == 31 || code == 131 || code == 132) {
        std::ostringstream asql;
        std::string fieldlist = "*";
	if (code == 31 || code == 131 || code == 132) {
		std::vector<std::string> names;
		for (int i = 4; i < static_cast<int>(fld.size()); ++i) {
			names.push_back(fld[i].name);
		}
		std::ostringstream oss;
		for (size_t i = 0; i < names.size(); ++i) {
			oss << names[i];
			if (i + 1 < names.size())
				oss << ",";
		}
		fieldlist = oss.str();
	}
        if (code == 132)
            asql << "SELECT " << fieldlist << " FROM " << default_table << " WHERE DATEN_SN=:param1";
        else
            asql << "SELECT " << fieldlist << " FROM " << default_table << " WHERE SN=:param1";
        FDQuery1->Params->Clear();
        FDQuery1->Params->CreateParam(ftString, "param1", ptInput);
        FDQuery1->ParamByName("param1")->AsString = fld[1].value.c_str();
        FDQuery1->SQL->Text = asql.str().c_str();
        if (!fdExecOpen(FDQuery1))
            return;
        String sre;
        for (int i = 0; i < 4; ++i) {
            sre += String(fld[i].value.c_str());
            sre += separator;
        }
        std::stringstream tmp;
        bool exitLoop = false;
        FDQuery1->First();
        while (!FDQuery1->Eof && !exitLoop) {
            for (int i = 0; i < FDQuery1->FieldDefs->Count; ++i) {
                switch (FDQuery1->FieldDefList->FieldDefs[i]->DataType) {
                case ftString:
                    sre += FDQuery1->FieldByName(FDQuery1->FieldDefList->FieldDefs[i]->Name)->AsString;
                    sre += separator;
                    break;
                case ftInteger:
                    sre += FDQuery1->FieldByName(FDQuery1->FieldDefList->FieldDefs[i]->Name)->AsInteger;
                    sre += separator;
                    break;
                case ftFloat: {
                    for (auto &a : fld) {
                        if (a.name.c_str() == FDQuery1->FieldDefList->FieldDefs[i]->Name) {
                            std::size_t start = a.type.find(".") + 1;
                            std::size_t end = a.type.find("f");
                            int fact = std::stoi(a.type.substr(start, end - start));
                            tmp.str(std::string());
                            tmp << std::fixed << std::setprecision(0)
                                << FDQuery1->FieldByName(FDQuery1->FieldDefList->FieldDefs[i]->Name)->AsFloat * std::pow(10, fact);
                            String t = tmp.str().c_str();
                            int val = t.ToIntDef(0);
                            std::ostringstream r;
                            if (val >= 0)
                                r << std::setw(10) << std::setfill('0') << val;
                            else {
                                val = -val;
                                r << '-' << std::setw(9) << std::setfill('0') << val;
                            }
                            sre += r.str().c_str();
                            sre += separator;
                            break;
                        }
                    }
                } break;
                case ftFixedChar:
                    sre += FDQuery1->FieldByName(FDQuery1->FieldDefList->FieldDefs[i]->Name)->AsString;
                    sre += separator;
                    break;
                default:
                    break;
                }
                if (exitLoop)
                    break;
			}
			if (!exitLoop)
                FDQuery1->Next();
        }
        SENDUDP(sre.c_str());
        return;
    }
    if (code == 50 || code == 51 || code == 52) {
        switch (code) {
        case 50:
            IdSMTP1->Host = Inifile->ReadString("EMAIL", "SMTP_PRIO1_HOST", "0");
            IdSMTP1->Username = Inifile->ReadString("EMAIL", "SMTP_PRIO1_USER", "");
            IdSMTP1->Password = Inifile->ReadString("EMAIL", "SMTP_PRIO1_PASS", "");
            IdMessage1->Recipients->EMailAddresses = Inifile->ReadString("EMAIL", "SMTP_PRIO1_Recipients", "");
            break;
        case 51:
            IdSMTP1->Host = Inifile->ReadString("EMAIL", "SMTP_PRIO2_HOST", "0");
            IdSMTP1->Username = Inifile->ReadString("EMAIL", "SMTP_PRIO2_USER", "");
            IdSMTP1->Password = Inifile->ReadString("EMAIL", "SMTP_PRIO2_PASS", "");
            IdMessage1->Recipients->EMailAddresses = Inifile->ReadString("EMAIL", "SMTP_PRIO2_Recipients", "");
            break;
        case 52:
			IdSMTP1->Host = Inifile->ReadString("EMAIL", "SMTP_PRIO3_HOST", "0");
            IdSMTP1->Username = Inifile->ReadString("EMAIL", "SMTP_PRIO3_USER", "");
            IdSMTP1->Password = Inifile->ReadString("EMAIL", "SMTP_PRIO3_PASS", "");
            IdMessage1->Recipients->EMailAddresses = Inifile->ReadString("EMAIL", "SMTP_PRIO3_Recipients", "");
            break;
        }
        IdMessage1->Clear();
        IdMessage1->Subject = "--NO--SUBJECT--";
        IdMessage1->Body->Text = "--TEXT--";
        IdMessage1->From->Name = "--FROM--SPS--";
        IdMessage1->From->Address = "harald.wolf@ulmer-automation.de";
        try {
            IdSMTP1->Connect();
            IdSMTP1->Send(IdMessage1);
        } catch (...) {
            ShowMessage(" error ... ");
        }
        IdSMTP1->Disconnect();
        return;
    }
    update_table_metadata(rdb_field_name, default_table);
    if (!rdb_field_name.size()) {
        std::ostringstream psql;
        psql << "CREATE TABLE " << default_table << " (ID BIGINT NOT NULL,ZEIT TIMESTAMP DEFAULT CURRENT_TIMESTAMP,DBK VARCHAR(5),SN VARCHAR(50),CODE VARCHAR(4), SPSK VARCHAR(4));\n";
        psql << "set term !! ;\n";
        psql << "CREATE TRIGGER TRIG" << default_table << " FOR " << default_table << "\nACTIVE BEFORE INSERT POSITION 0\nAS BEGIN\nif (NEW.ID is NULL) then NEW.ID = GEN_ID(GEN, 1);\nEND!!";
        ExecuteSQL(psql.str().c_str());
    }
    if (code == 133) {
        std::ostringstream ss;
        ss << "INSERT INTO " << default_table << " (DBK,CODE,SPSK) VALUES('" << tok[0] << "','0133','" << tok[3] << "') returning id {into :new_id}";
        FDQuery1->SQL->Text = ss.str().c_str();
        try {
            FDQuery1->Params->CreateParam(ftByte, "new_id", ptOutput)->DataType = ftBytes;
            if (!fdExecOpen(FDQuery1))
                return;
            int new_id = FDQuery1->FieldByName("id")->AsInteger;
            FDQuery1->Close();
            std::ostringstream antwort;
            antwort << tok[0] << separator << new_id << separator << tok[2] << separator << tok[3] << separator << default_table << separator;
            DspLogAdd("Neue ID Angefordert.", __LINE__, clLime);
            SENDUDP(antwort.str().c_str());
        } catch (EFDException &e) {
            DspLogAdd(e.Message, __LINE__, clRed);
        }
        return;
    }
    for (auto &a : fld) {
        if (a.name == "ID" || a.name == "ZEIT" || a.name == "DBK" || a.name == "SN" || a.name == "CODE" || a.name == "SPSK")
            continue;
        if (rdb_field_name[a.name] == 0) {
            std::ostringstream asql;
            asql << "ALTER TABLE " << default_table << " ADD " << a.name.c_str();
            if (a.type == "%s")
                asql << " VARCHAR(255)";
            else if (a.type == "%d")
                asql << " INT";
            else if (a.type == "%u")
                asql << " BIGINT";
            else if (a.type == "%c")
                asql << " CHAR(1)";
            else if ((a.type.rfind("%.", 0) == 0 && a.type.back() == 'f') || a.type == "%f")
                asql << " DOUBLE PRECISION";
            else {
                DspLogAdd("Datenblock falsch angegeben: " + String(asql.str().c_str()) + "\tFormat " + String(a.type.c_str()) + " Ungültig", __LINE__, clRed);
                DspLogAdd("Feldtrenner " + String(separator) + " prüfen!", __LINE__, clRed);
                return;
            }
            ExecuteSQL(asql.str().c_str());
        }
    }
    std::ostringstream flds, vals;
    String insert_method;
    if (code == 20 || code == 120 || code == 23 || code == 123)
        insert_method = "UPDATE_OR_INSERT";
    if (code == 22) {
        insert_method = "UPDATE_OR_INSERT";
        UnicodeString d = AnsiString(Now().FormatString("yymmdd")).c_str();
        Inifile->WriteInteger("HUMMEL81", "ENGELSN", Inifile->ReadInteger("HUMMEL81", "ENGELSN", 1) + 1);
        if (Inifile->ReadString("HUMMEL81", "ENGELLASTDATE", "") != d) {
            Inifile->WriteString("HUMMEL81", "ENGELLASTDATE", d);
            Inifile->WriteString("HUMMEL81", "ENGELSN", "1");
            Inifile->UpdateFile();
        }
        std::ostringstream ss;
        ss << fld[1].value.substr(0, 10) << AnsiString(d).c_str() << std::setw(4) << std::setfill('0') << AnsiString(Inifile->ReadString("HUMMEL81", "ENGELSN", 1)).c_str();
        fld[1].value = ss.str();
        ss.str(std::string());
        for (auto &a : fld)
            ss << a.value << separator;
        s = ss.str();
    }
    if (insert_method == "UPDATE_OR_INSERT")
        flds << "UPDATE OR";
    flds << " INSERT INTO " << default_table << " (";
    if (code == 23 || code == 123) {
        insert_method = "UPDATE";
        flds.str(std::string());
        auto m_id = fld[1];
        fld.erase(fld.begin() + 1);
        flds << "UPDATE " << default_table << " SET ";
        int fldno = 0;
        for (auto &a : fld) {
            flds << a.name.c_str() << "="
                 << ":param" << fldno++ << (fldno == fld.size() ? "" : ",");
        }
        if (code == 123 || code == 133)
            flds << " WHERE ID=" << m_id.value;
        else
            flds << " WHERE ID=x'" << m_id.value << "'";
    } else {
        int fldno = 0;
        for (auto &a : fld) {
            flds << a.name.c_str() << ",";
            vals << ":param" << fldno++ << ",";
        }
        flds << ") VALUES (" << vals.str() << ")";
        if (code == 20 || code == 120)
            flds << " MATCHING(SN) RETURNING SN";
    }
    if (code == 23 || code == 123)
        flds << " returning id {into :new_id}";
    std::string fsql = flds.str();
    for (std::size_t pos = 0; (pos = fsql.find(",)")) != std::string::npos;)
        fsql.replace(pos, 2, ")");
    int col = 0;
	for (auto &a : fld) {
        String pname = "param" + String(col);
        if (a.type == "%s") {
            FDQuery1->Params->CreateParam(ftString, pname, ptInput);
            FDQuery1->ParamByName(pname)->AsString = a.value.c_str();
        } else if (a.type == "%d") {
            FDQuery1->Params->CreateParam(ftInteger, pname, ptInput);
            FDQuery1->ParamByName(pname)->AsInteger = std::atoi(a.value.c_str());
        } else if (a.type == "%u") {
			FDQuery1->Params->CreateParam(ftLargeint, pname, ptInput);
            FDQuery1->ParamByName(pname)->AsInteger = std::atoi(a.value.c_str());
        } else if ((a.type.rfind("%.", 0) == 0 && a.type.back() == 'f') || a.type == "%f") {
            std::size_t start = a.type.find(".") + 1;
            std::size_t end = a.type.find("f");
            int fact = std::stoi(a.type.substr(start, end - start));
            double val = std::stod(a.value.c_str()) * std::pow(10, -fact);
            FDQuery1->Params->CreateParam(ftFloat, pname, ptInput);
            FDQuery1->ParamByName(pname)->AsFloat = val;
        } else {
            if (!a.type.empty() && col > 3) {
                DspLogAdd("Unbekannter Datentyp!", __LINE__, clRed);
                return;
            }
            FDQuery1->Params->CreateParam(ftString, pname, ptInput);
            FDQuery1->ParamByName(pname)->AsString = a.value.c_str();
        }
        ++col;
    }
    FDQuery1->SQL->Text = fsql.c_str();
    if (code == 23 || code == 123) {
        try {
            FDQuery1->Params->CreateParam(ftByte, "new_id", ptOutput)->DataType = ftBytes;
            if (!fdExecOpen(FDQuery1))
                return;
            TBytes idBytes = FDQuery1->FieldByName("id")->AsBytes;
            std::string hex = idBytesToHex(idBytes);
            if (!hex.empty())
                SENDUDP(s.c_str());
            return;
        } catch (EFDException &e) {
            DspLogAdd(e.Message, __LINE__, clRed);
        }
    } else {
        if (!fdExecSQL(FDQuery1))
            return;
    }
    FDQuery1->Close();
    SENDUDP(s.c_str());
    start = std::chrono::system_clock::now();
    ErrorStatus = 1;
    StatusBar1->Panels->Items[1]->Text = AnsiString(entries++) + " Aufrufe";
    if (entries > 100000)
        entries = 0;
}
