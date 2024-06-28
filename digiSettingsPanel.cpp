#include "digiSettingsPanel.h"

#include <QLabel>
#include <QFileDialog>
#include <QStyledItemDelegate>
#include <QToolTip>
#include <QPoint>

std::vector<std::pair<std::string, Reg>> infoIndex = {{"Serial Num : ",            PHA::DIG::SerialNumber},
                                                      {"IP : ",                    PHA::DIG::IPAddress},
                                                      {"Model Name : ",            PHA::DIG::ModelName},
                                                      {"FPGA version : ",          PHA::DIG::FPGA_firmwareVersion},
                                                      {"DPP Type : ",              PHA::DIG::FirmwareType},
                                                      {"CUP version : ",           PHA::DIG::CupVer},
                                                      {"ADC bits : ",              PHA::DIG::ADC_bit},
                                                      {"ADC rate [Msps] : ",       PHA::DIG::ADC_SampleRate},
                                                      {"Num. of Channel : ",       PHA::DIG::NumberOfChannel},
                                                      {"Input range [Vpp] : ",     PHA::DIG::InputDynamicRange},
                                                      {"Input Type : ",            PHA::DIG::InputType},
                                                      {"Input Impedance [Ohm] : ", PHA::DIG::InputImpedance}
                                                    };

QStringList LEDToolTip = { "LED_JESD_Y_PASS" ,
                        "LED_JESD_H_PASS" ,
                        "LED_DDR4_0_PASS" ,
                        "LED_DDR4_1_PASS" ,
                        "LED_DDR4_2_PASS" ,
                        "LEDFP_FAIL"     ,
                        "LEDFP_NIM"     ,
                        "LEDFP_TTL"     ,
                        "LEDFP_DTLOSS"    ,
                        "LEDFP_DTRDY"     ,
                        "LEDFP_TRG"       ,
                        "LEDFP_RUN"       ,
                        "LEDFP_PLL_LOCK"  ,
                        "LEDFP_CLKOUT"   ,
                        "LEDFP_CLKIN"    ,
                        "LEDFP_USB"      ,
                        "LEDFP_SFP_SD"   ,
                        "LEDFP_SFP_ACT"  ,
                        "LEDFP_ACT"     } ;

QStringList ACQToolTip = {"Armed",
                       "Run",
                       "Run_mw",
                       "Jesd_Clk_Valid",
                       "Busy",
                       "PreTriggerReady",
                       "LicenceFail" };

QStringList chToolTip = { "Channel signal delay initialization status (1 = initial delay done)",
                       "Channel time filter initialization status (1 = time filter initialization done)",
                       "Channel energy filter initialization status (1 = energy filter initialization done)",
                       "Channel full initialization status (1 = initialization done)",
                       "Reserved",
                       "Channel enable acquisition status (1 = acq enabled)",
                       "Channel inner run status (1 = run active)",
                       "Time-energy event free space status (1 = time-energy can be written)",
                       "Waveform event free space status (1 = waveform can be written)"};

QColor orangeColor(255, 165, 0);

DigiSettingsPanel::DigiSettingsPanel(Digitizer2Gen ** digi, unsigned short nDigi, QString analysisPath, QWidget * parent) : QWidget(parent){

  setWindowTitle("Digitizers Settings");
  setGeometry(0, 0, 1850, 1050);
  //setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  this->digi = digi;
  this->nDigi = nDigi;
  if( nDigi > MaxNumberOfDigitizer ) {
    this->nDigi = MaxNumberOfDigitizer;
    qDebug() << "Please increase the MaxNumberOfChannel";
  }
  this->digiSettingPath = analysisPath + "/working/Settings/";

  ID = 0;
  enableSignalSlot = false;
  QVBoxLayout * mainLayout = new QVBoxLayout(this); this->setLayout(mainLayout);
  tabWidget = new QTabWidget(this); mainLayout->addWidget(tabWidget);

  //@========================== Tab for each digitizer
  for(unsigned short iDigi = 0; iDigi < this->nDigi; iDigi++){

    QScrollArea * scrollArea = new QScrollArea(this); 
    scrollArea->setWidgetResizable(true);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    tabWidget->addTab(scrollArea, "Digi-" + QString::number(digi[iDigi]->GetSerialNumber()));

    digiTab[iDigi] = new QWidget(tabWidget);
    scrollArea->setWidget(digiTab[iDigi]);
    
    QHBoxLayout * tabLayout_H  = new QHBoxLayout(digiTab[iDigi]); 
    QVBoxLayout * tabLayout_V1 = new QVBoxLayout(); tabLayout_H->addLayout(tabLayout_V1);
    QVBoxLayout * tabLayout_V2 = new QVBoxLayout(); tabLayout_H->addLayout(tabLayout_V2);
    
    {//^====================== Group of Digitizer Info
      QGroupBox * infoBox = new QGroupBox("Board Info", digiTab[iDigi]);
      //infoBox->setSizePolicy(sizePolicy);

      QGridLayout * infoLayout = new QGridLayout(infoBox);
      tabLayout_V1->addWidget(infoBox);
      
      const unsigned short nRow = 4;
      for( unsigned short j = 0; j < (unsigned short) infoIndex.size(); j++){
        QLabel * lab = new QLabel(QString::fromStdString(infoIndex[j].first), digiTab[iDigi]);
        lab->setAlignment(Qt::AlignRight | Qt::AlignCenter);
        leInfo[iDigi][j] = new QLineEdit(digiTab[iDigi]);
        leInfo[iDigi][j]->setReadOnly(true);

        Reg reg = infoIndex[j].second;
        QString text = QString::fromStdString(digi[iDigi]->ReadValue(reg));
        if( reg.GetPara() == PHA::DIG::ADC_SampleRate.GetPara() ) {
          text += " = " + QString::number(digi[iDigi]->GetTick2ns(), 'f', 1) + " ns" ;
        }
        leInfo[iDigi][j]->setText(text);
        infoLayout->addWidget(lab, j%nRow, 2*(j/nRow));
        infoLayout->addWidget(leInfo[iDigi][j], j%nRow, 2*(j/nRow) +1);
      }
    }

    {//^====================== Group Board status
      QGroupBox * statusBox = new QGroupBox("Board Status", digiTab[iDigi]);
      QGridLayout * statusLayout = new QGridLayout(statusBox);
      statusLayout->setAlignment(Qt::AlignLeft);
      statusLayout->setHorizontalSpacing(0);

      tabLayout_V1->addWidget(statusBox);

      //------- LED Status
      QLabel * lbLED = new QLabel("LED status : ");
      lbLED->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      statusLayout->addWidget(lbLED, 0, 0);

      for( int i = 0; i < 19; i++){
        LEDStatus[iDigi][i] = new QPushButton(digiTab[iDigi]);
        LEDStatus[iDigi][i]->setEnabled(false);
        LEDStatus[iDigi][i]->setFixedSize(QSize(30,30));
        LEDStatus[iDigi][i]->setToolTip(QString::number(i) + " - " + LEDToolTip[i]);
        LEDStatus[iDigi][i]->setToolTipDuration(-1);
        //TODO set tooltip position on top
        statusLayout->addWidget(LEDStatus[iDigi][i], 0, 19 - i);
      
      }

      //------- ACD Status
      QLabel * lbACQ = new QLabel("ACQ status : ");
      lbACQ->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      statusLayout->addWidget(lbACQ, 1, 0);

      for( int i = 0; i < 7; i++){
        ACQStatus[iDigi][i] = new QPushButton(digiTab[iDigi]);
        ACQStatus[iDigi][i]->setEnabled(false);
        ACQStatus[iDigi][i]->setFixedSize(QSize(30,30));
        ACQStatus[iDigi][i]->setToolTip(QString::number(i) + " - " + ACQToolTip[i]);
        ACQStatus[iDigi][i]->setToolTipDuration(-1);
        statusLayout->addWidget(ACQStatus[iDigi][i], 1,  7 - i);
      }

      //------- Temperatures
      QLabel * lbTemp = new QLabel("ADC Temperature [C] : ");
      lbTemp->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      statusLayout->addWidget(lbTemp, 2, 0);

      for( int i = 0; i < 8; i++){
        leTemp[iDigi][i] = new QLineEdit(digiTab[iDigi]);
        leTemp[iDigi][i]->setReadOnly(true);
        leTemp[iDigi][i]->setAlignment(Qt::AlignHCenter);
        statusLayout->addWidget(leTemp[iDigi][i], 2, 1 + 2*i, 1, 2);
      }

      QPushButton * bnUpdateStatus = new QPushButton("Update Status", this);
      statusLayout->addWidget(bnUpdateStatus, 2, 19);
      connect(bnUpdateStatus, &QPushButton::clicked, this, &DigiSettingsPanel::UpdateStatus);

      for( int i = 0; i < statusLayout->columnCount(); i++) statusLayout->setColumnStretch(i, 0 );
    }

    {//^====================== Board Setting Buttons
      QGridLayout * bnLayout = new QGridLayout();
      tabLayout_V1->addLayout(bnLayout);
    
      int rowId = 0;
      //-------------------------------------
      QLabel * lbSettingFile = new QLabel("Setting File : ", digiTab[iDigi]);
      lbSettingFile->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      bnLayout->addWidget(lbSettingFile, rowId, 0);

      leSettingFile[iDigi] = new QLineEdit(digiTab[iDigi]);
      leSettingFile[iDigi]->setReadOnly(true);
      leSettingFile[iDigi]->setText(QString::fromStdString(digi[iDigi]->GetSettingFileName()));
      bnLayout->addWidget(leSettingFile[iDigi], rowId, 1, 1, 9);
      
      //-------------------------------------
      rowId ++;
      bnReadSettngs[iDigi] = new QPushButton("Refresh Settings", digiTab[iDigi]);
      bnLayout->addWidget(bnReadSettngs[iDigi], rowId, 0, 1, 2);
      connect(bnReadSettngs[iDigi], &QPushButton::clicked, this, &DigiSettingsPanel::RefreshSettings);
      
      bnResetBd[iDigi] = new QPushButton("Reset Board", digiTab[iDigi]);
      bnLayout->addWidget(bnResetBd[iDigi], rowId, 2, 1, 2);
      connect(bnResetBd[iDigi], &QPushButton::clicked, this, [=](){
         SendLogMsg("Reset Digitizer-" + QString::number(digi[ID]->GetSerialNumber()));
         digi[ID]->Reset();    
         RefreshSettings();
      });
      
      bnDefaultSetting[iDigi] = new QPushButton("Set Default Settings", digiTab[iDigi]);
      bnLayout->addWidget(bnDefaultSetting[iDigi], rowId, 4, 1, 2);
      connect(bnDefaultSetting[iDigi], &QPushButton::clicked, this, &DigiSettingsPanel::SetDefaultPHASettigns);

      bnSaveSettings[iDigi] = new QPushButton("Save Settings", digiTab[iDigi]);
      bnLayout->addWidget(bnSaveSettings[iDigi], rowId, 6, 1, 2);
      connect(bnSaveSettings[iDigi], &QPushButton::clicked, this, &DigiSettingsPanel::SaveSettings);

      bnLoadSettings[iDigi] = new QPushButton("Load Settings", digiTab[iDigi]);
      bnLayout->addWidget(bnLoadSettings[iDigi], rowId, 8, 1, 2);
      connect(bnLoadSettings[iDigi], &QPushButton::clicked, this, &DigiSettingsPanel::LoadSettings);

      //---------------------------------------
      rowId ++;
      bnClearData[iDigi] = new QPushButton("Clear Data", digiTab[iDigi]);
      bnLayout->addWidget(bnClearData[iDigi], rowId, 0, 1, 2);
      connect(bnClearData[iDigi], &QPushButton::clicked, this, [=](){ 
        digi[ID]->SendCommand(PHA::DIG::ClearData);
        SendLogMsg("Digi-" + QString::number(digi[ID]->GetSerialNumber()) + "|Send Command : " + QString::fromStdString(PHA::DIG::ClearData.GetFullPara()));
        UpdateStatus();
      });
      
      bnArmACQ[iDigi] = new QPushButton("Arm ACQ", digiTab[iDigi]);
      bnLayout->addWidget(bnArmACQ[iDigi], rowId, 2, 1, 2);
      connect(bnArmACQ[iDigi], &QPushButton::clicked, this, [=](){ 
        digi[ID]->SendCommand(PHA::DIG::ArmACQ); 
        SendLogMsg("Digi-" + QString::number(digi[ID]->GetSerialNumber()) + "|Send Command : " + QString::fromStdString(PHA::DIG::ArmACQ.GetFullPara()));
        UpdateStatus();
      });
      
      bnDisarmACQ[iDigi] = new QPushButton("Disarm ACQ", digiTab[iDigi]);
      bnLayout->addWidget(bnDisarmACQ[iDigi], rowId, 4, 1, 2);
      connect(bnDisarmACQ[iDigi], &QPushButton::clicked, this, [=](){ 
        digi[ID]->SendCommand(PHA::DIG::DisarmACQ); 
        SendLogMsg("Digi-" + QString::number(digi[ID]->GetSerialNumber()) + "|Send Command : " + QString::fromStdString(PHA::DIG::DisarmACQ.GetFullPara()));
        UpdateStatus();
      });

      bnSoftwareStart[iDigi] = new QPushButton("Software Start ACQ", digiTab[iDigi]);
      bnLayout->addWidget(bnSoftwareStart[iDigi], rowId, 6, 1, 2);
      connect(bnSoftwareStart[iDigi], &QPushButton::clicked, this, [=](){ 
        digi[ID]->SendCommand(PHA::DIG::SoftwareStartACQ); 
        SendLogMsg("Digi-" + QString::number(digi[ID]->GetSerialNumber()) + "|Send Command : " + QString::fromStdString(PHA::DIG::SoftwareStartACQ.GetFullPara()));
        UpdateStatus();
      });

      bnSoftwareStop[iDigi] = new QPushButton("Software Stop ACQ", digiTab[iDigi]);
      bnLayout->addWidget(bnSoftwareStop[iDigi], rowId, 8, 1, 2);
      connect(bnSoftwareStop[iDigi], &QPushButton::clicked, this, [=](){ 
        digi[ID]->SendCommand(PHA::DIG::SoftwareStopACQ); 
        SendLogMsg("Digi-" + QString::number(digi[ID]->GetSerialNumber()) + "|Send Command : " + QString::fromStdString(PHA::DIG::SoftwareStopACQ.GetFullPara()));
        UpdateStatus();
      });

      //--------------- 
      if( digi[iDigi]->IsDummy() || !digi[iDigi]->IsConnected() ){
        bnReadSettngs[iDigi]->setEnabled(false);
        bnResetBd[iDigi]->setEnabled(false);
        bnDefaultSetting[iDigi]->setEnabled(false);
        bnClearData[iDigi]->setEnabled(false);
        bnArmACQ[iDigi]->setEnabled(false);
        bnDisarmACQ[iDigi]->setEnabled(false);
        bnSoftwareStart[iDigi]->setEnabled(false);
        bnSoftwareStop[iDigi]->setEnabled(false);
      }

    }


    {//^============================ Board Settings tab

      QTabWidget * bdTab = new QTabWidget(digiTab[iDigi]);
      tabLayout_V1->addWidget(bdTab);

      {//^====================== Group Board settings

        bdCfg[iDigi] = new QWidget(this);
        bdTab->addTab(bdCfg[iDigi], "Board");

        //digiBox[iDigi] = new QGroupBox("Board Settings", digiTab[iDigi]);
        // //digiBox->setSizePolicy(sizePolicy);
        QGridLayout * boardLayout = new QGridLayout(bdCfg[iDigi]);
        boardLayout->setAlignment(Qt::AlignTop);
        boardLayout->setSpacing(2);
        //tabLayout_V1->addWidget(digiBox[iDigi]);
          
        int rowId = 0;
        //-------------------------------------
        SetupComboBox(cbbClockSource[iDigi], PHA::DIG::ClockSource, -1, false, "Clock Source :", boardLayout, rowId, 0, 1, 2);

        QLabel * lbEnClockFP = new QLabel("Enable Clock Out Font Panel :", digiTab[iDigi]);
        lbEnClockFP->setAlignment(Qt::AlignRight | Qt::AlignCenter);
        boardLayout->addWidget(lbEnClockFP, rowId, 2, 1, 3);

        cbbEnClockFrontPanel[iDigi] = new RComboBox(digiTab[iDigi]);
        boardLayout->addWidget(cbbEnClockFrontPanel[iDigi], rowId, 5);
        SetupShortComboBox(cbbEnClockFrontPanel[iDigi], PHA::DIG::EnableClockOutFrontPanel);
        connect(cbbEnClockFrontPanel[iDigi], &RComboBox::currentIndexChanged, this, [=](){
          if( !enableSignalSlot ) return;
          //printf("%s %d  %s \n", para.GetPara().c_str(), ch_index, cbb->currentData().toString().toStdString().c_str());
          QString msg;
          msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(PHA::DIG::EnableClockOutFrontPanel.GetPara());
          msg += " = " + cbbEnClockFrontPanel[ID]->currentData().toString();
          if( digi[ID]->WriteValue(PHA::DIG::EnableClockOutFrontPanel, cbbEnClockFrontPanel[ID]->currentData().toString().toStdString())){
            SendLogMsg(msg + "|OK.");
            cbbEnClockFrontPanel[ID]->setStyleSheet("");
          }else{
            SendLogMsg(msg + "|Fail.");
            cbbEnClockFrontPanel[ID]->setStyleSheet("color:red;");
          }
        });


        //-------------------------------------
        rowId ++;
        QLabel * lbStartSource = new QLabel("Start Source :", digiTab[iDigi]);
        lbStartSource->setAlignment(Qt::AlignRight);
        boardLayout->addWidget(lbStartSource, rowId, 0);

        for( int i = 0; i < (int) PHA::DIG::StartSource.GetAnswers().size(); i++){
          ckbStartSource[iDigi][i] = new QCheckBox( QString::fromStdString((PHA::DIG::StartSource.GetAnswers())[i].second), digiTab[iDigi]);
          boardLayout->addWidget(ckbStartSource[iDigi][i], rowId, 1 + i);
          connect(ckbStartSource[iDigi][i], &QCheckBox::stateChanged, this, &DigiSettingsPanel::SetStartSource);
        }

        //-------------------------------------
        rowId ++;
        QLabel * lbGlobalTrgSource = new QLabel("Global Trigger Source :", digiTab[iDigi]);
        lbGlobalTrgSource->setAlignment(Qt::AlignRight);
        boardLayout->addWidget(lbGlobalTrgSource, rowId, 0);

        for( int i = 0; i < (int) PHA::DIG::GlobalTriggerSource.GetAnswers().size(); i++){
          ckbGlbTrgSource[iDigi][i] = new QCheckBox( QString::fromStdString((PHA::DIG::GlobalTriggerSource.GetAnswers())[i].second), digiTab[iDigi]);
          boardLayout->addWidget(ckbGlbTrgSource[iDigi][i], rowId, 1 + i);
          connect(ckbGlbTrgSource[iDigi][i], &QCheckBox::stateChanged, this, &DigiSettingsPanel::SetGlobalTriggerSource);
        }

        //-------------------------------------
        rowId ++;
        SetupComboBox(cbbTrgOut[iDigi], PHA::DIG::TrgOutMode, -1, false, "Trg-OUT Mode :", boardLayout, rowId, 0, 1, 2);
              
        //-------------------------------------
        rowId ++;
        SetupComboBox(cbbGPIO[iDigi], PHA::DIG::GPIOMode, -1, false, "GPIO Mode :", boardLayout, rowId, 0, 1, 2);

        //-------------------------------------
        QLabel * lbAutoDisarmAcq = new QLabel("Auto disarm ACQ :", digiTab[iDigi]);
        lbAutoDisarmAcq->setAlignment(Qt::AlignRight);
        boardLayout->addWidget(lbAutoDisarmAcq, rowId, 4, 1, 2);

        cbbAutoDisarmAcq[iDigi] = new RComboBox(digiTab[iDigi]);
        boardLayout->addWidget(cbbAutoDisarmAcq[iDigi], rowId, 6);
        SetupShortComboBox(cbbAutoDisarmAcq[iDigi], PHA::DIG::EnableAutoDisarmACQ);
        connect(cbbAutoDisarmAcq[iDigi], &RComboBox::currentIndexChanged, this, [=](){
          if( !enableSignalSlot ) return;
          //printf("%s %d  %s \n", para.GetPara().c_str(), ch_index, cbb->currentData().toString().toStdString().c_str());
          QString msg;
          msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(PHA::DIG::EnableAutoDisarmACQ.GetPara());
          msg += " = " + cbbAutoDisarmAcq[ID]->currentData().toString();
          if( digi[ID]->WriteValue(PHA::DIG::EnableAutoDisarmACQ, cbbAutoDisarmAcq[ID]->currentData().toString().toStdString())){
            SendLogMsg(msg + "|OK.");
            cbbAutoDisarmAcq[ID]->setStyleSheet("");
          }else{
            SendLogMsg(msg + "|Fail.");
            cbbAutoDisarmAcq[ID]->setStyleSheet("color:red;");
          }
        });

        //-------------------------------------
        rowId ++;
        SetupComboBox(cbbBusyIn[iDigi], PHA::DIG::BusyInSource, -1, false, "Busy In Source :", boardLayout, rowId, 0, 1, 2);

        QLabel * lbStatEvents = new QLabel("Stat. Event :", digiTab[iDigi]);
        lbStatEvents->setAlignment(Qt::AlignRight);
        boardLayout->addWidget(lbStatEvents, rowId, 4, 1, 2);

        cbbStatEvents[iDigi] = new RComboBox(digiTab[iDigi]);
        boardLayout->addWidget(cbbStatEvents[iDigi], rowId, 6);
        SetupShortComboBox(cbbStatEvents[iDigi], PHA::DIG::EnableStatisticEvents);
        connect(cbbStatEvents[iDigi], &RComboBox::currentIndexChanged, this, [=](){
          if( !enableSignalSlot ) return;
          QString msg;
          msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(PHA::DIG::EnableStatisticEvents.GetPara());
          msg += " = " + cbbStatEvents[ID]->currentData().toString();
          if( digi[ID]->WriteValue(PHA::DIG::EnableStatisticEvents, cbbStatEvents[ID]->currentData().toString().toStdString()) ){
            SendLogMsg(msg + "|OK.");
            cbbStatEvents[ID]->setStyleSheet("");
          }else{
            SendLogMsg(msg + "|Fail.");
            cbbStatEvents[ID]->setStyleSheet("color:red");
          }
        });

        //-------------------------------------
        rowId ++;
        SetupComboBox(cbbSyncOut[iDigi], PHA::DIG::SyncOutMode, -1, false, "Sync Out mode :", boardLayout, rowId, 0, 1, 2);

        //-------------------------------------
        rowId ++;
        SetupComboBox(cbbBoardVetoSource[iDigi], PHA::DIG::BoardVetoSource, -1, false, "Board Veto Source :", boardLayout, rowId, 0, 1, 2);

        QLabel * lbBdVetoWidth = new QLabel("Board Veto Width [ns] :", digiTab[iDigi]);
        lbBdVetoWidth->setAlignment(Qt::AlignRight);
        boardLayout->addWidget(lbBdVetoWidth, rowId, 3, 1, 2);

        dsbBdVetoWidth[iDigi] = new RSpinBox(digiTab[iDigi], 0); // may be QDoubleSpinBox
        dsbBdVetoWidth[iDigi]->setMinimum(0);
        dsbBdVetoWidth[iDigi]->setMaximum(34359738360);
        dsbBdVetoWidth[iDigi]->setSingleStep(20);
        dsbBdVetoWidth[iDigi]->SetToolTip();
        boardLayout->addWidget(dsbBdVetoWidth[iDigi], rowId, 5);
        connect(dsbBdVetoWidth[iDigi], &RSpinBox::valueChanged, this, [=](){
          if( !enableSignalSlot ) return;
          dsbBdVetoWidth[ID]->setStyleSheet("color:blue;");
        });
        connect(dsbBdVetoWidth[iDigi], &RSpinBox::returnPressed, this, [=](){
          if( !enableSignalSlot ) return;
          //printf("%s %d  %d \n", para.GetPara().c_str(), ch_index, spb->value());
          QString msg;
          msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(PHA::DIG::BoardVetoWidth.GetPara());
          msg += " = " + QString::number(dsbBdVetoWidth[iDigi]->value());
          if( digi[ID]->WriteValue(PHA::DIG::BoardVetoWidth, std::to_string(dsbBdVetoWidth[iDigi]->value()), -1) ){
            dsbBdVetoWidth[ID]->setStyleSheet("");
            SendLogMsg(msg + "|OK.");
          }else{
            dsbBdVetoWidth[ID]->setStyleSheet("color:red;");
            SendLogMsg(msg + "|Fail.");
          }
        });

        cbbBdVetoPolarity[iDigi] = new RComboBox(digiTab[iDigi]);
        boardLayout->addWidget(cbbBdVetoPolarity[iDigi], rowId, 6);
        SetupShortComboBox(cbbBdVetoPolarity[iDigi], PHA::DIG::BoardVetoPolarity);
        connect(cbbBdVetoPolarity[iDigi], &RComboBox::currentIndexChanged, this, [=](){
          if( !enableSignalSlot ) return;
          QString msg;
          msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(PHA::DIG::BoardVetoPolarity.GetPara());
          msg += " = " + cbbBdVetoPolarity[ID]->currentData().toString();
          if( digi[ID]->WriteValue(PHA::DIG::BoardVetoPolarity, cbbBdVetoPolarity[ID]->currentData().toString().toStdString()) ){
            SendLogMsg(msg + "|OK.");
            cbbBdVetoPolarity[ID]->setStyleSheet("");
          }else{
            SendLogMsg(msg + "|Fail.");
            cbbBdVetoPolarity[ID]->setStyleSheet("color:red");
          }
        });

        
        //-------------------------------------
        rowId ++;
        SetupSpinBox(spbRunDelay[iDigi], PHA::DIG::RunDelay, -1, false, "Run Delay [ns] :", boardLayout, rowId, 0);

        //-------------------------------------
        QLabel * lbClockOutDelay = new QLabel("Temp. Clock Out Delay [ps] :", digiTab[iDigi]);
        lbClockOutDelay->setAlignment(Qt::AlignRight);
        boardLayout->addWidget(lbClockOutDelay, rowId, 3, 1, 2);

        dsbVolatileClockOutDelay[iDigi] = new RSpinBox(digiTab[iDigi], 3);
        dsbVolatileClockOutDelay[iDigi]->setMinimum(-18888.888);
        dsbVolatileClockOutDelay[iDigi]->setMaximum(18888.888);
        dsbVolatileClockOutDelay[iDigi]->setSingleStep(74.074);
        dsbVolatileClockOutDelay[iDigi]->setValue(0);
        dsbVolatileClockOutDelay[iDigi]->SetToolTip();
        boardLayout->addWidget(dsbVolatileClockOutDelay[iDigi], rowId, 5);
        connect(dsbVolatileClockOutDelay[iDigi], &RSpinBox::valueChanged, this, [=](){
          if( !enableSignalSlot ) return;
          dsbVolatileClockOutDelay[ID]->setStyleSheet("color:blue;");
        });
        connect(dsbVolatileClockOutDelay[iDigi], &RSpinBox::returnPressed, this, [=](){
          if( !enableSignalSlot ) return;
          //printf("%s %d  %d \n", para.GetPara().c_str(), ch_index, spb->value());
          double step = dsbVolatileClockOutDelay[ID]->singleStep();
          double value = dsbVolatileClockOutDelay[ID]->value();
          dsbVolatileClockOutDelay[ID]->setValue( (std::round(value/step) * step) );
          QString msg;
          msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(PHA::DIG::VolatileClockOutDelay.GetPara());
          msg += " = " + QString::number(dsbVolatileClockOutDelay[iDigi]->value());
          if( digi[ID]->WriteValue(PHA::DIG::VolatileClockOutDelay, std::to_string(dsbVolatileClockOutDelay[ID]->value()), -1) ){
            dsbVolatileClockOutDelay[ID]->setStyleSheet("");
            SendLogMsg(msg + "|OK.");
          }else{
            dsbVolatileClockOutDelay[ID]->setStyleSheet("color:red;");
            SendLogMsg(msg + "|Fail.");
          }
        });

        //-------------------------------------
        rowId ++;
        SetupComboBox(cbbIOLevel[iDigi], PHA::DIG::IO_Level, -1, false, "IO Level :", boardLayout, rowId, 0, 1, 2);

        QLabel * lbClockOutDelay2 = new QLabel("Perm. Clock Out Delay [ps] :", digiTab[iDigi]);
        lbClockOutDelay2->setAlignment(Qt::AlignRight);
        boardLayout->addWidget(lbClockOutDelay2, rowId, 3, 1, 2);

        dsbClockOutDelay[iDigi] = new RSpinBox(digiTab[iDigi], 3);
        dsbClockOutDelay[iDigi]->setMinimum(-18888.888);
        dsbClockOutDelay[iDigi]->setMaximum(18888.888);
        dsbClockOutDelay[iDigi]->setValue(0);
        dsbClockOutDelay[iDigi]->setSingleStep(74.074);
        dsbClockOutDelay[iDigi]->SetToolTip();
        boardLayout->addWidget(dsbClockOutDelay[iDigi], rowId, 5);
        connect(dsbClockOutDelay[iDigi], &RSpinBox::valueChanged, this, [=](){
          if( !enableSignalSlot ) return;
          dsbClockOutDelay[ID]->setStyleSheet("color:blue;");
        });
        connect(dsbClockOutDelay[iDigi], &RSpinBox::returnPressed, this, [=](){
          if( !enableSignalSlot ) return;
          //printf("%s %d  %d \n", para.GetPara().c_str(), ch_index, spb->value());
          double step = dsbClockOutDelay[ID]->singleStep();
          double value = dsbClockOutDelay[ID]->value();
          dsbClockOutDelay[ID]->setValue( (std::round(value/step) * step) );
          QString msg;
          msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(PHA::DIG::PermanentClockOutDelay.GetPara());
          msg += " = " + QString::number(dsbClockOutDelay[iDigi]->value());
          if( digi[ID]->WriteValue(PHA::DIG::PermanentClockOutDelay, std::to_string(dsbClockOutDelay[ID]->value()), -1) ){
            dsbClockOutDelay[ID]->setStyleSheet("");
            SendLogMsg(msg + "|OK.");
          }else{
            dsbClockOutDelay[ID]->setStyleSheet("color:red;");
            SendLogMsg(msg + "|Fail.");
          }
        });

        //-------------------------------------
        rowId ++;
        SetupComboBox(cbDACoutMode[iDigi], PHA::DIG::DACoutMode, -1, false, "DAC out Mode :", boardLayout, rowId, 0, 1, 2);

        //-------------------------------------
        rowId ++;
        SetupSpinBox(sbDACoutStaticLevel[iDigi], PHA::DIG::DACoutStaticLevel, -1, false, "DAC Static Lv. :", boardLayout, rowId, 0, 1, 2);

        //-------------------------------------
        rowId ++;
        SetupSpinBox(sbDACoutChSelect[iDigi], PHA::DIG::DACoutChSelect, -1, false, "DAC Channel :", boardLayout, rowId, 0, 1, 2);

        connect(cbDACoutMode[iDigi], &RComboBox::currentIndexChanged, this, [=](){
          if( cbDACoutMode[iDigi]->currentData().toString().toStdString() == "Static" ) {
            sbDACoutStaticLevel[iDigi]->setEnabled(true);
          }else{
            sbDACoutStaticLevel[iDigi]->setEnabled(false);
          }
        });

        connect(cbDACoutMode[iDigi], &RComboBox::currentIndexChanged, this, [=](){
          if( cbDACoutMode[iDigi]->currentData().toString().toStdString() == "ChInput"  ) {
            sbDACoutChSelect[iDigi]->setEnabled(true);
          }else{
            sbDACoutChSelect[iDigi]->setEnabled(false);
          }
        });

        connect(cbDACoutMode[iDigi], &RComboBox::currentIndexChanged, this, [=](){
          if( cbDACoutMode[iDigi]->currentData().toString().toStdString() == "Square" ) {
            bdTestPulse[iDigi]->setEnabled(true);
          }else{
            if( ckbGlbTrgSource[iDigi][3]->isChecked() == false )  bdTestPulse[iDigi]->setEnabled(false);
          }
        });

      }
      
      {//^====================== Test Pulse settings

        bdTestPulse[iDigi] = new QWidget(this);
        bdTab->addTab(bdTestPulse[iDigi], "Test Pulse");
        QGridLayout * testPulseLayout = new QGridLayout(bdTestPulse[iDigi]);
        testPulseLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        testPulseLayout->setSpacing(2);

        SetupSpinBox(dsbTestPuslePeriod[iDigi], PHA::DIG::TestPulsePeriod, -1, false, "Period [ns] :", testPulseLayout, 0, 0);
        SetupSpinBox(dsbTestPusleWidth[iDigi], PHA::DIG::TestPulseWidth, -1, false, "Width [ns] :", testPulseLayout, 1, 0);
        SetupSpinBox(spbTestPusleLowLevel[iDigi], PHA::DIG::TestPulseLowLevel, -1, false, "Low Lvl. [LSB] :", testPulseLayout, 2, 0);
        SetupSpinBox(spbTestPusleHighLevel[iDigi], PHA::DIG::TestPulseHighLevel, -1, false, "High Lvl. [LSB] :", testPulseLayout, 3, 0);

        dsbTestPuslePeriod[iDigi]->setFixedWidth(100);
        dsbTestPusleWidth[iDigi]->setFixedWidth(100);
        spbTestPusleLowLevel[iDigi]->setFixedWidth(100);
        spbTestPusleHighLevel[iDigi]->setFixedWidth(100);

        QLabel * lblow = new QLabel("", bdTestPulse[iDigi]); lblow->setAlignment(Qt::AlignCenter); testPulseLayout->addWidget(lblow, 2, 2);
        QLabel * lbhigh = new QLabel("", bdTestPulse[iDigi]); lbhigh->setAlignment(Qt::AlignCenter); testPulseLayout->addWidget(lbhigh, 3, 2);
        
        connect(spbTestPusleLowLevel[iDigi], &RSpinBox::valueChanged, this, [=](){
          double value = spbTestPusleLowLevel[iDigi]->value();
          lblow->setText("approx. " + QString::number(value/0xffff*2 - 1, 'f', 2) + " V");
        });

        connect(spbTestPusleHighLevel[iDigi], &RSpinBox::valueChanged, this, [=](){
          double value = spbTestPusleHighLevel[iDigi]->value();
          lbhigh->setText("approx. " + QString::number(value/0xffff*2 - 1, 'f', 2) + " V");
        });

      }

      {//^====================== VGA settings

        bdVGA[iDigi] = new QWidget(this);
        bdTab->addTab(bdVGA[iDigi], "VGA Setting");
        
        QGridLayout * vgaLayout = new QGridLayout(bdVGA[iDigi]);
        //vgaLayout->setVerticalSpacing(0);
        vgaLayout->setAlignment(Qt::AlignTop| Qt::AlignLeft);

        for( int k = 0; k < 4; k ++){
          SetupSpinBox(VGA[iDigi][k], PHA::VGA::VGAGain, k, false, "VGA-" + QString::number(k) + " [dB] :", vgaLayout, k, 0);
          VGA[iDigi][k]->setSingleStep(0.5);
          VGA[iDigi][k]->setFixedWidth(100);
          VGA[iDigi][k]->SetToolTip();
          
        }
      }
    
      {//^====================== ITL
        bdITL[iDigi] = new QWidget(this);
        bdTab->addTab(bdITL[iDigi], "ITL-A/B");
        QGridLayout * ITLLayout = new QGridLayout(bdITL[iDigi]);
        ITLLayout->setAlignment(Qt::AlignTop);

        QGroupBox * gbITLA = new QGroupBox("ITL-A", bdITL[iDigi]);
        ITLLayout->addWidget(gbITLA, 0, 0);
        QGridLayout * aLayout = new QGridLayout(gbITLA);

        SetupComboBox(cbITLAMainLogic[iDigi], PHA::DIG::ITLAMainLogic,   -1, false, "Main Logic", aLayout, 0, 0);
        SetupSpinBox( sbITLAMajority[iDigi],  PHA::DIG::ITLAMajorityLev, -1, false, "Majority", aLayout, 1, 0);
        SetupComboBox(cbITLAPairLogic[iDigi], PHA::DIG::ITLAPairLogic,   -1, false, "Pair Logic", aLayout, 2, 0);
        SetupComboBox(cbITLAPolarity[iDigi],  PHA::DIG::ITLAPolarity,    -1, false, "Polarity", aLayout, 3, 0);
        SetupSpinBox( sbITLAGateWidth[iDigi], PHA::DIG::ITLAGateWidth,   -1, false, "Output GateWidth [ns]", aLayout, 4, 0);

        connect(cbITLAMainLogic[iDigi], &RComboBox::currentIndexChanged, this, [=](){
          sbITLAMajority[iDigi]->setEnabled(cbITLAMainLogic[iDigi]->currentData().toString() == "Majority");
        });
      
        QGroupBox * gbITLB = new QGroupBox("ITL-B", bdITL[iDigi]);
        ITLLayout->addWidget(gbITLB, 0, 1);
        QGridLayout * bLayout = new QGridLayout(gbITLB);

        SetupComboBox(cbITLBMainLogic[iDigi], PHA::DIG::ITLBMainLogic,   -1, false, "Main Logic", bLayout, 0, 0);
        SetupSpinBox( sbITLBMajority[iDigi],  PHA::DIG::ITLBMajorityLev, -1, false, "Majority", bLayout, 1, 0);
        SetupComboBox(cbITLBPairLogic[iDigi], PHA::DIG::ITLBPairLogic,   -1, false, "Pair Logic", bLayout, 2, 0);
        SetupComboBox(cbITLBPolarity[iDigi],  PHA::DIG::ITLBPolarity,    -1, false, "Polarity", bLayout, 3, 0);
        SetupSpinBox( sbITLBGateWidth[iDigi], PHA::DIG::ITLBGateWidth,   -1, false, "Output GateWidth [ns]", bLayout, 4, 0);

        connect(cbITLBMainLogic[iDigi], &RComboBox::currentIndexChanged, this, [=](){
          sbITLBMajority[iDigi]->setEnabled(cbITLBMainLogic[iDigi]->currentData().toString() == "Majority");
        });

        QGroupBox * gbITL = new QGroupBox("ITL-Connect", bdITL[iDigi]);
        ITLLayout->addWidget(gbITL, 1, 0, 1, 2);
        QGridLayout * cLayout = new QGridLayout(gbITL);

        for( int i = 0; i < 32; i++){
          if( i % 3 == 0  || i == 31){
            QLabel * haha = new QLabel(QString::number(i), bdITL[iDigi]); cLayout->addWidget(haha, 0, i + 1);
          }
        }

        QLabel * lb1 = new QLabel("ITL-A", bdITL[iDigi]);
        cLayout->addWidget(lb1, 1, 0, 2, 1);

        QLabel * lb2 = new QLabel("ITL-B", bdITL[iDigi]);
        cLayout->addWidget(lb2, 4, 0, 2, 1);

        QFrame *horizontalSeparator = new QFrame();
        horizontalSeparator->setFrameShape(QFrame::HLine);
        horizontalSeparator->setFrameShadow(QFrame::Sunken);
        cLayout->addWidget(horizontalSeparator, 3, 0, 1, 33);


        for( int i = 0; i < 64; i++){
          chITLConnect[iDigi][i][0] = new QPushButton(bdITL[iDigi]);
          chITLConnect[iDigi][i][0]->setFixedSize(15, 15);
          cLayout->addWidget(chITLConnect[iDigi][i][0], 1 + i/32, i%32 + 1);

          chITLConnect[iDigi][i][1] = new QPushButton(bdITL[iDigi]);
          chITLConnect[iDigi][i][1]->setFixedSize(15, 15);
          cLayout->addWidget(chITLConnect[iDigi][i][1], 4 + i/32, i%32 + 1);

          connect(chITLConnect[iDigi][i][0], &QPushButton::clicked, this, [=](){
            //printf(" %d ch %d clicked, %d \n", iDigi, i, ITLConnectStatus[iDigi][i]);
            if( (ITLConnectStatus[iDigi][i] & 0x1) == 0 ){
              ITLConnectStatus[iDigi][i] += 1;
              chITLConnect[iDigi][i][0]->setStyleSheet("background-color : green;");
              if( ((ITLConnectStatus[iDigi][i] >> 1) & 0x1) == 1 ) {
                ITLConnectStatus[iDigi][i] -= 2;
                chITLConnect[iDigi][i][1]->setStyleSheet("");
              }
            }else{
              ITLConnectStatus[iDigi][i] -= 1;
              chITLConnect[iDigi][i][0]->setStyleSheet("");
            }

            std::string value = "Disabled";
            if( ITLConnectStatus[iDigi][i] == 1 ) value = "ITLA";
            if( ITLConnectStatus[iDigi][i] == 2 ) value = "ITLB";

            QString msg;
            msg = "DIG:" + QString::number(digi[ID]->GetNChannels()) + ",CH:" + QString::number(i) + "|" + QString::fromStdString(PHA::CH::ITLConnect.GetPara() ) + " = " + QString::fromStdString(value);
            if( digi[ID]->WriteValue(PHA::CH::ITLConnect, value, i) ){
              SendLogMsg(msg + "|OK.");
            }else{
              SendLogMsg(msg + "|Fail.");
              digi[ID]->ReadValue(PHA::CH::ITLConnect, i);
              chITLConnect[iDigi][i][0]->setStyleSheet("background-color : red;");
            }

          });

          connect(chITLConnect[iDigi][i][1], &QPushButton::clicked, this, [=](){
            //printf(" %d ch %d clicked, %d \n", iDigi, i, ITLConnectStatus[iDigi][i]);
            if( ((ITLConnectStatus[iDigi][i] >> 1) & 0x1) == 0 ){
              ITLConnectStatus[iDigi][i] += 2;
              chITLConnect[iDigi][i][1]->setStyleSheet("background-color : green;");

              if( (ITLConnectStatus[iDigi][i] & 0x1) == 1 ) {
                ITLConnectStatus[iDigi][i] -= 1;
                chITLConnect[iDigi][i][0]->setStyleSheet("");
              }

            }else{
              ITLConnectStatus[iDigi][i] -= 2;
              chITLConnect[iDigi][i][1]->setStyleSheet("");
            }

            std::string value = "Disabled";
            if( ITLConnectStatus[iDigi][i] == 1 ) value = "ITLA";
            if( ITLConnectStatus[iDigi][i] == 2 ) value = "ITLB";

            QString msg;
            msg = "DIG:" + QString::number(digi[ID]->GetNChannels()) + ",CH:" + QString::number(i) + "|" + QString::fromStdString(PHA::CH::ITLConnect.GetPara() ) + " = " + QString::fromStdString(value);
            if( digi[ID]->WriteValue(PHA::CH::ITLConnect, value, i) ){
              SendLogMsg(msg + "|OK.");
            }else{
              SendLogMsg(msg + "|Fail.");
              digi[ID]->ReadValue(PHA::CH::ITLConnect, i);
              chITLConnect[iDigi][i][1]->setStyleSheet("background-color : red;");
            }
          });

        }

      } 

      {//^====================== LVDS
        bdLVDS[iDigi] = new QWidget(this);
        bdTab->addTab(bdLVDS[iDigi], "LVDS");
        QGridLayout * LVDSLayout = new QGridLayout(bdLVDS[iDigi]);
        LVDSLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        //LVDSLayout->setSpacing(2);

        for(int k = 0; k < 4; k ++){
          SetupComboBox(cbLVDSMode[iDigi][k], PHA::LVDS::LVDSMode, k, false, "Ch-" + QString::number(16* k) + ":" + QString::number(16*(k+1)-1) + "  Mode :", LVDSLayout, k, 0);
          SetupComboBox(cbLVDSDirection[iDigi][k], PHA::LVDS::LVDSDirection, k, false, "Direction :", LVDSLayout, k, 2);
        }

        QLabel * lbIOReg = new QLabel("Status :", bdLVDS[iDigi]);
        lbIOReg->setAlignment(Qt::AlignCenter | Qt::AlignRight);
        LVDSLayout->addWidget(lbIOReg, 4, 0);

        leLVDSIOReg[iDigi] = new QLineEdit(bdLVDS[iDigi]);
        LVDSLayout->addWidget(leLVDSIOReg[iDigi], 4, 1, 1, 3);
        connect(leLVDSIOReg[iDigi], &QLineEdit::textChanged, this, [=](){if( enableSignalSlot ) leLVDSIOReg[iDigi]->setStyleSheet("color: green;");});
        connect(leLVDSIOReg[iDigi], &QLineEdit::returnPressed, this, [=](){
          if( !enableSignalSlot ) return;
          std::string value = leLVDSIOReg[iDigi]->text().toStdString();
          Reg para = PHA::DIG::LVDSIOReg;

          QString msg;
          msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(para.GetPara());
          msg += " = " + leLVDSIOReg[iDigi]->text();
          if( digi[ID]->WriteValue(para, value)){
            SendLogMsg(msg + "|OK.");
            leLVDSIOReg[iDigi]->setStyleSheet("");
            UpdatePanelFromMemory();
            UpdateOtherPanels();
          }else{
            SendLogMsg(msg + "|Fail.");
            leLVDSIOReg[iDigi]->setStyleSheet("color:red;");
          }
        });

      }

      {//^====================== Group = InputDelay
        bdGroup[iDigi] = new QWidget(this);
        bdTab->addTab(bdGroup[iDigi], "Input Delay");
        QGridLayout * groupLayout = new QGridLayout(bdGroup[iDigi]);
        groupLayout->setAlignment(Qt::AlignTop );
        //LVDSLayout->setSpacing(2);

        for(int k = 0; k < MaxNumberOfGroup; k ++){
          SetupSpinBox(spbInputDelay[iDigi][k], PHA::GROUP::InputDelay, k, false, "ch : " + QString::number(4*k) + " - " + QString::number(4*k+3) + " [ns] ", groupLayout, k/4, 2*(k%4));
        }

        bdGroup[iDigi]->setEnabled(digi[iDigi]->GetCupVer() >= MIN_VERSION_GROUP);
      }

    }

    {//^====================== Group channel settings
      QGroupBox * chBox = new QGroupBox("Channel Settings", digiTab[iDigi]); 
      //chBox->setSizePolicy(sizePolicy);
      tabLayout_V2->addWidget(chBox);
      QGridLayout * chLayout = new QGridLayout(chBox); //chBox->setLayout(chLayout);
      chBox->setFixedWidth(900);

      chTabWidget[iDigi] = new QTabWidget(digiTab[iDigi]); chLayout->addWidget(chTabWidget[iDigi]);

      if( digi[iDigi]->GetFPGAType() == DPPType::PHA ) SetupPHAChannels(iDigi);
      if( digi[iDigi]->GetFPGAType() == DPPType::PSD ) SetupPSDChannels(iDigi);

      {//@============== Status  tab
        QTabWidget * statusTab = new QTabWidget(digiTab[iDigi]);
        chTabWidget[iDigi]->addTab(statusTab, "Status");

        QGridLayout * layout = new QGridLayout(statusTab);
        layout->setAlignment(Qt::AlignTop);
        layout->setSpacing(0);

        QLabel* lb0x = new QLabel("9-bit status", statusTab); lb0x->setAlignment(Qt::AlignHCenter); layout->addWidget(lb0x, 0, 1, 1, 9);
        QLabel* lb0a = new QLabel("Gain Fact.", statusTab); lb0a->setAlignment(Qt::AlignHCenter); layout->addWidget(lb0a, 0, 10);
        QLabel* lb1a = new QLabel("ADC->Volt", statusTab); lb1a->setAlignment(Qt::AlignHCenter); layout->addWidget(lb1a, 0, 11);

        QLabel* lb1x = new QLabel("9-bit status", statusTab); lb1x->setAlignment(Qt::AlignHCenter); layout->addWidget(lb1x, 0, 13, 1, 9);
        QLabel* lb0b = new QLabel("Gain Fact.", statusTab); lb0b->setAlignment(Qt::AlignHCenter); layout->addWidget(lb0b, 0, 22);
        QLabel* lb1b = new QLabel("ADC->Volt", statusTab); lb1b->setAlignment(Qt::AlignHCenter); layout->addWidget(lb1b, 0, 23);
        
        for( int ch = 0; ch < digi[iDigi]->GetNChannels(); ch ++){
          QLabel * lb = new QLabel("  ch-" + QString::number(ch) + "  ", statusTab);
          lb->setAlignment(Qt::AlignRight | Qt::AlignCenter);
          layout->addWidget(lb, 1 + ch/2, ch%2 * 12);

          for( int k = 0 ; k < 9; k++){
            chStatus[iDigi][ch][k] = new QPushButton(statusTab);
            chStatus[iDigi][ch][k]->setEnabled(false);
            chStatus[iDigi][ch][k]->setFixedSize(QSize(10,25));
            chStatus[iDigi][ch][k]->setToolTip(QString::number(k) + " - " + chToolTip[k]);
            chStatus[iDigi][ch][k]->setToolTipDuration(-1);
            layout->addWidget(chStatus[iDigi][ch][k], 1 + ch/2, ch%2 * 12 + 9 - k); // arrange backward, so that it is like a bit-wise
          }

          chGainFactor[iDigi][ch] = new QLineEdit(statusTab);
          chGainFactor[iDigi][ch]->setFixedWidth(100);
          layout->addWidget(chGainFactor[iDigi][ch], 1 + ch/2, ch%2*12 + 10);

          chADCToVolts[iDigi][ch] = new QLineEdit(statusTab);
          chADCToVolts[iDigi][ch]->setFixedWidth(100);
          layout->addWidget(chADCToVolts[iDigi][ch], 1 + ch/2, ch%2*12 + 11);

        }
      }

      {//@============== Trigger Mask/Map  tab

        triggerMapTab[iDigi] = new QTabWidget(digiTab[iDigi]);
        chTabWidget[iDigi]->addTab(triggerMapTab[iDigi], "Trigger Map");

        QGridLayout * triggerLayout = new QGridLayout(triggerMapTab[iDigi]);
        triggerLayout->setAlignment(Qt::AlignTop);

        int rowID = 0;
        //----------------------------
        // SetupComboBox( cbAllEvtTrigger[iDigi], PHA::CH::EventTriggerSource, -1, false, "Event Trigger Source (all ch.)", triggerLayout, rowID, 0);
        // SetupComboBox( cbAllWaveTrigger[iDigi], PHA::CH::WaveTriggerSource, -1, false, "Wave Trigger Source (all ch.)", triggerLayout, rowID, 2);

        // //----------------------------
        // rowID ++;
        // SetupComboBox( cbAllCoinMask[iDigi], PHA::CH::CoincidenceMask, -1, false, "Coincident Mask (all ch.)", triggerLayout, rowID, 0);
        // SetupSpinBox( sbAllCoinLength[iDigi], PHA::CH::CoincidenceLength, -1, false, "Coincident Length [ns] (all ch.)", triggerLayout, rowID, 2);
        
        // //----------------------------
        // rowID ++;
        // SetupComboBox( cbAllAntiCoinMask[iDigi], PHA::CH::AntiCoincidenceMask, -1, false, "Anti-Coincident Mask (all ch.)", triggerLayout, rowID, 0);

        QSignalMapper * triggerMapper = new QSignalMapper(digiTab[iDigi]);
        connect(triggerMapper, &QSignalMapper::mappedInt, this, &DigiSettingsPanel::onTriggerClick);

        //----------------------------
        rowID ++;
        QGroupBox * triggerBox = new QGroupBox("Trigger Mask", digiTab[iDigi]);
        triggerLayout->addWidget(triggerBox, rowID, 0, 1, 4);

        QGridLayout * tbLayout = new QGridLayout(triggerBox);
        tbLayout->setAlignment(Qt::AlignCenter | Qt::AlignTop);
        tbLayout->setSpacing(0);

        //----------------------------
        rowID = 0;
        QLabel * instr1 = new QLabel("Reading: Column (C) represents a trigger channel for Row (R) channel.", digiTab[iDigi]);
        instr1->setAlignment(Qt::AlignLeft);
        tbLayout->addWidget(instr1, rowID, 0, 1, 64+15);        
        
        rowID ++;
        QLabel * instr2 = new QLabel("For example, R3C1 = ch-3 trigger source is ch-1.", digiTab[iDigi]);
        instr2->setAlignment(Qt::AlignLeft);
        tbLayout->addWidget(instr2, rowID, 0, 1, 64+15);

        rowID ++;
        for( int j = 0; j < digi[iDigi]->GetNChannels(); j++){
          if( j % 4 == 0) {
            QLabel * lllb = new QLabel(QString::number(j), digiTab[iDigi]);
            lllb->setAlignment(Qt::AlignLeft);
            tbLayout->addWidget(lllb, rowID , 1 + j + j/4, 1, 4); 
          }
        }

        rowID ++;
        int colID = 0;
        for(int i = 0; i < digi[iDigi]->GetNChannels(); i++){
          colID = 1;

          if( i % 4 == 0){
            QLabel * lllba = new QLabel(QString::number(i), digiTab[iDigi]);
            lllba->setAlignment(Qt::AlignTop | Qt::AlignRight);
            tbLayout->addWidget(lllba, 3 + i + i/4, 0, 4, 1); 
          }

          for(int j = 0; j < digi[iDigi]->GetNChannels(); j++){

            trgMap[iDigi][i][j] = new QPushButton(digiTab[iDigi]);
            trgMap[iDigi][i][j]->setFixedSize(QSize(10,10));
            trgMapClickStatus[iDigi][i][j] = false;
            tbLayout->addWidget(trgMap[iDigi][i][j], rowID, colID);

            triggerMapper->setMapping(trgMap[iDigi][i][j], (iDigi << 16) + (i << 8) + j);
            connect(trgMap[iDigi][i][j], SIGNAL(clicked()), triggerMapper, SLOT(map()));

            colID ++;
            if( j%4 == 3 && j!= digi[iDigi]->GetNChannels() - 1){
              QFrame * vSeparator = new QFrame(digiTab[iDigi]);
              vSeparator->setFrameShape(QFrame::VLine);
              tbLayout->addWidget(vSeparator, rowID, colID);
              colID++;
            }
          }

          rowID++;
          if( i%4 == 3 && i != digi[iDigi]->GetNChannels() - 1){
            QFrame * hSeparator = new QFrame(digiTab[iDigi]);
            hSeparator->setFrameShape(QFrame::HLine);
            tbLayout->addWidget(hSeparator, rowID, 1, 1, digi[iDigi]->GetNChannels() + 15);
            rowID++;
          }
        }
      }

    } //=== end of channel group

  } //=== end of digiTab[iDigi]

  {//^============================================== Inquiry / Copy Tab
    ICTab = new QTabWidget(this);
    tabWidget->addTab(ICTab, "Inquiry / Copy");

    QVBoxLayout * layout1 = new QVBoxLayout(ICTab);
    layout1->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    {//@============== low level inquiry
      icBox1 = new QGroupBox("Low Level Settings", ICTab);
      layout1->addWidget(icBox1);

      QGridLayout * inquiryLayout = new QGridLayout(icBox1);
      inquiryLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
      //inquiryLayout->setSpacing(0);    

      int rowID = 0;
      QLabel * info1 = new QLabel("This will read from the digitizer, save the setting to memory.", icBox1);
      inquiryLayout->addWidget(info1, rowID, 0, 1, 6);
      rowID ++;
      QLabel * info2 = new QLabel("The settings from Digitizer Tab will not reflect immedinately in here.", icBox1);
      inquiryLayout->addWidget(info2, rowID, 0, 1, 6);

      ///=========== header
      rowID ++;
      QLabel * hd0 = new QLabel("Digi. Value", icBox1);
      hd0->setAlignment(Qt::AlignCenter);
      inquiryLayout->addWidget(hd0, rowID, 3);
      
      QLabel * hd1 = new QLabel("Unit", icBox1);
      hd1->setAlignment(Qt::AlignCenter);
      inquiryLayout->addWidget(hd1, rowID, 4);
      
      QLabel * hd2 = new QLabel("<---------------------- Set Value ---------------------->", icBox1);
      hd2->setAlignment(Qt::AlignCenter);
      inquiryLayout->addWidget(hd2, rowID, 6, 1, 3);

      ///========================== board settings
      rowID ++;
      cbIQDigi = new RComboBox(ICTab);
      for( int i = 0; i < nDigi; i++) cbIQDigi->addItem("Digi-" + QString::number(digi[i]->GetSerialNumber()), i);
      inquiryLayout->addWidget(cbIQDigi, rowID, 0);
      connect( cbIQDigi, &RComboBox::currentIndexChanged, this, [=](int index){
        enableSignalSlot = false;
        ID = index;
        cbIQCh->clear();
        for( int i = 0; i < digi[index]->GetNChannels() ; i++ ) cbIQCh->addItem( "Ch-" + QString::number(i), i);

        cbBdSettings->clear();
        cbChSettings->clear();
        if( digi[ID]->GetFPGAType() == DPPType::PHA ){
          for( int i = 0; i < (int) PHA::DIG::AllSettings.size(); i++ ){
            cbBdSettings->addItem( QString::fromStdString( PHA::DIG::AllSettings[i].GetPara() ), i);
          }
          for( int i = 0; i < (int) PHA::CH::AllSettings.size(); i++ ){
            cbChSettings->addItem( QString::fromStdString( PHA::CH::AllSettings[i].GetPara() ), i);
          }
        }
        if( digi[ID]->GetFPGAType() == DPPType::PSD ){
          for( int i = 0; i < (int) PSD::DIG::AllSettings.size(); i++ ){
            cbBdSettings->addItem( QString::fromStdString( PSD::DIG::AllSettings[i].GetPara() ), i);
          }
          for( int i = 0; i < (int) PSD::CH::AllSettings.size(); i++ ){
            cbChSettings->addItem( QString::fromStdString( PSD::CH::AllSettings[i].GetPara() ), i);
          }
        }



        enableSignalSlot = true;
      });

      cbBdSettings = new RComboBox(ICTab);
      if( digi[0]->GetFPGAType() == DPPType::PHA ){
        for( int i = 0; i < (int) PHA::DIG::AllSettings.size(); i++ ){
          cbBdSettings->addItem( QString::fromStdString( PHA::DIG::AllSettings[i].GetPara() ), i);
        }
      }
      if( digi[ID]->GetFPGAType() == DPPType::PSD ){
        for( int i = 0; i < (int) PSD::DIG::AllSettings.size(); i++ ){
          cbBdSettings->addItem( QString::fromStdString( PSD::DIG::AllSettings[i].GetPara() ), i);
        }
      }

      inquiryLayout->addWidget(cbBdSettings, rowID, 1);
      connect(cbBdSettings, &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::ReadBoardSetting);

      leBdSettingsType = new QLineEdit("Read Only",ICTab);
      leBdSettingsType->setAlignment(Qt::AlignHCenter);
      leBdSettingsType->setFixedWidth(150);
      leBdSettingsType->setReadOnly(true);
      inquiryLayout->addWidget(leBdSettingsType, rowID, 2);

      leBdSettingsRead = new QLineEdit(ICTab);
      leBdSettingsRead->setAlignment(Qt::AlignRight);
      leBdSettingsRead->setFixedWidth(250);
      leBdSettingsRead->setReadOnly(true);
      inquiryLayout->addWidget(leBdSettingsRead, rowID, 3);

      leBdSettingsUnit = new QLineEdit("", ICTab);
      leBdSettingsUnit->setAlignment(Qt::AlignHCenter);
      leBdSettingsUnit->setFixedWidth(50);
      leBdSettingsUnit->setReadOnly(true);
      inquiryLayout->addWidget(leBdSettingsUnit, rowID, 4);

      QPushButton * bnRead = new QPushButton("Read", ICTab);
      inquiryLayout->addWidget(bnRead, rowID, 5, 2, 1);
      connect(bnRead, &QPushButton::clicked, this, [=](){ ReadBoardSetting(cbBdSettings->currentIndex()); ReadChannelSetting(cbChSettings->currentIndex()); });

      cbBdAns = new RComboBox(ICTab);
      cbBdAns->setFixedWidth(200);
      inquiryLayout->addWidget(cbBdAns, rowID, 6);
      connect(cbBdAns, &RComboBox::currentIndexChanged, this, [=](){
        if( !enableSignalSlot ) return;
        std::string value = cbBdAns->currentData().toString().toStdString();
        leBdSettingsWrite->setText(QString::fromStdString(value));
        leBdSettingsWrite->setStyleSheet("");

        Reg para = PHA::DIG::AllSettings[cbBdSettings->currentIndex()];
        ID = cbIQDigi->currentIndex();
        QString msg;
        msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(para.GetPara());
        msg += " = " + cbBdAns->currentData().toString();
        if( digi[ID]->WriteValue(para, value) ){
          leBdSettingsRead->setText( QString::fromStdString(digi[ID]->GetSettingValueFromMemory(para)));
          SendLogMsg(msg + "|OK.");
          cbBdAns->setStyleSheet("");
          UpdatePanelFromMemory();
          UpdateOtherPanels();
        }else{
          leBdSettingsRead->setText("fail write value");
          SendLogMsg(msg + "|Fail.");
          cbBdAns->setStyleSheet("color:red;");
        }
      });

      sbBdSettingsWrite = new RSpinBox(ICTab);
      sbBdSettingsWrite->setFixedWidth(200);
      inquiryLayout->addWidget(sbBdSettingsWrite, rowID, 7);
      connect(sbBdSettingsWrite, &RSpinBox::valueChanged, this, [=](){if( enableSignalSlot ) sbBdSettingsWrite->setStyleSheet("color: green;");});
      connect(sbBdSettingsWrite, &RSpinBox::returnPressed, this, [=](){ 
        if( !enableSignalSlot ) return;
        if( sbBdSettingsWrite->decimals() == 0 && sbBdSettingsWrite->singleStep() != 1) {
          double step = sbBdSettingsWrite->singleStep();
          double value = sbBdSettingsWrite->value();
          sbBdSettingsWrite->setValue( (std::round(value/step) * step) );
        }

        Reg para = PHA::DIG::AllSettings[cbBdSettings->currentIndex()];
        ID = cbIQDigi->currentIndex();
        QString msg;
        msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(para.GetPara());
        msg += " = " + QString::number(sbBdSettingsWrite->value());
        if( digi[ID]->WriteValue(para, std::to_string(sbBdSettingsWrite->value()))){
          leBdSettingsRead->setText( QString::fromStdString(digi[ID]->GetSettingValueFromMemory(para)));
          SendLogMsg(msg + "|OK.");
          sbBdSettingsWrite->setStyleSheet("");
          UpdatePanelFromMemory();
          UpdateOtherPanels();
        }else{
          leBdSettingsRead->setText("fail write value");
          SendLogMsg(msg + "|Fail.");
          sbBdSettingsWrite->setStyleSheet("color:red;");
        }
      });
      
      leBdSettingsWrite = new QLineEdit(ICTab);
      leBdSettingsWrite->setAlignment(Qt::AlignRight);
      leBdSettingsWrite->setFixedWidth(200);
      inquiryLayout->addWidget(leBdSettingsWrite, rowID, 8);
      connect(leBdSettingsWrite, &QLineEdit::textChanged, this, [=](){if( enableSignalSlot )leBdSettingsWrite->setStyleSheet("color: blue;");});
      connect(leBdSettingsWrite, &QLineEdit::returnPressed, this, [=](){
        if( !enableSignalSlot ) return;
        std::string value = leBdSettingsWrite->text().toStdString();
        Reg para = PHA::DIG::AllSettings[cbBdSettings->currentIndex()];
        ID = cbIQDigi->currentIndex();
        QString msg;
        msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(para.GetPara());
        msg += " = " + QString::number(sbBdSettingsWrite->value());
        if( digi[ID]->WriteValue(para, value)){
          leBdSettingsRead->setText( QString::fromStdString(digi[ID]->GetSettingValueFromMemory(para)));
          SendLogMsg(msg + "|OK.");
          sbBdSettingsWrite->setStyleSheet("");
          UpdatePanelFromMemory();
          UpdateOtherPanels();
        }else{
          leBdSettingsRead->setText("fail write value");
          SendLogMsg(msg + "|Fail.");
          sbBdSettingsWrite->setStyleSheet("color:red;");
        }
      });


      ///========================== Channels settings
      rowID ++;
      cbIQCh = new RComboBox(ICTab);
      cbIQCh->clear();
      for( int i = 0; i < digi[0]->GetNChannels() ; i++) cbIQCh->addItem("Ch-" + QString::number(i), i);
      inquiryLayout->addWidget(cbIQCh, rowID, 0);
      connect(cbIQCh, &RComboBox::currentIndexChanged, this, [=](){ ReadChannelSetting(cbChSettings->currentIndex()); });

      cbChSettings = new RComboBox(ICTab);
      if( digi[0]->GetFPGAType() == DPPType::PHA){
        for( int i = 0; i < (int) PHA::CH::AllSettings.size(); i++ ){
          cbChSettings->addItem( QString::fromStdString( PHA::CH::AllSettings[i].GetPara() ), i);
        }
      }
      if( digi[0]->GetFPGAType() == DPPType::PSD){
        for( int i = 0; i < (int) PSD::CH::AllSettings.size(); i++ ){
          cbChSettings->addItem( QString::fromStdString( PSD::CH::AllSettings[i].GetPara() ), i);
        }
      }
      inquiryLayout->addWidget(cbChSettings, rowID, 1);
      connect(cbChSettings, &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::ReadChannelSetting);

      leChSettingsType = new QLineEdit("Read Only", ICTab);
      leChSettingsType->setAlignment(Qt::AlignHCenter);
      leChSettingsType->setFixedWidth(150);
      leChSettingsType->setReadOnly(true);
      inquiryLayout->addWidget(leChSettingsType, rowID, 2);

      leChSettingsRead = new QLineEdit(ICTab);
      leChSettingsRead->setAlignment(Qt::AlignRight);
      leChSettingsRead->setFixedWidth(250);
      leChSettingsRead->setReadOnly(true);
      inquiryLayout->addWidget(leChSettingsRead, rowID, 3);
      
      leChSettingsUnit = new QLineEdit(ICTab);
      leChSettingsUnit->setAlignment(Qt::AlignHCenter);
      leChSettingsUnit->setFixedWidth(50);
      leChSettingsUnit->setReadOnly(true);
      inquiryLayout->addWidget(leChSettingsUnit, rowID, 4);

      cbChSettingsWrite = new RComboBox(ICTab);
      cbChSettingsWrite->setFixedWidth(200);
      inquiryLayout->addWidget(cbChSettingsWrite, rowID, 6);
      connect(cbChSettingsWrite, &RComboBox::currentIndexChanged, this, [=](){
        if( !enableSignalSlot ) return;
        std::string value = cbChSettingsWrite->currentData().toString().toStdString();
        leChSettingsWrite->setText(QString::fromStdString(value));
        leChSettingsWrite->setStyleSheet("");

        Reg para = PHA::CH::AllSettings[cbChSettings->currentIndex()];
        ID = cbIQDigi->currentIndex();
        int ch_index = cbIQCh->currentIndex();
        QString msg;
        msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(para.GetPara());
        msg += ",CH:" + QString::number(ch_index);
        msg += " = " + cbChSettingsWrite->currentData().toString();
        if( digi[ID]->WriteValue(para, value, ch_index) ){
          leChSettingsRead->setText( QString::fromStdString(digi[ID]->GetSettingValueFromMemory(para)));
          SendLogMsg(msg + "|OK.");
          cbChSettingsWrite->setStyleSheet("");
          UpdatePanelFromMemory();
          UpdateOtherPanels();
        }else{
          leChSettingsRead->setText("fail write value");
          SendLogMsg(msg + "|Fail.");
          cbChSettingsWrite->setStyleSheet("color:red;");
        }
      });

      sbChSettingsWrite = new RSpinBox(ICTab);
      sbChSettingsWrite->setFixedWidth(200);
      inquiryLayout->addWidget(sbChSettingsWrite, rowID, 7);
      connect(sbChSettingsWrite, &RSpinBox::valueChanged, this, [=](){ if( enableSignalSlot ) sbChSettingsWrite->setStyleSheet("color: green;");});
      connect(sbChSettingsWrite, &RSpinBox::returnPressed, this, [=](){ 
        if( !enableSignalSlot ) return;
        if( sbChSettingsWrite->decimals() == 0 && sbChSettingsWrite->singleStep() != 1) {
          double step = sbChSettingsWrite->singleStep();
          double value = sbChSettingsWrite->value();
          sbChSettingsWrite->setValue( (std::round(value/step) * step) );
        }

        Reg para = PHA::CH::AllSettings[cbChSettings->currentIndex()];
        ID = cbIQDigi->currentIndex();
        int ch_index = cbIQCh->currentIndex();
        QString msg;
        msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(para.GetPara());
        msg += ",CH:" + QString::number(ch_index);
        msg += " = " + QString::number(sbChSettingsWrite->value());
        if( digi[ID]->WriteValue(para, std::to_string(sbChSettingsWrite->value()), ch_index)){
          leChSettingsRead->setText( QString::fromStdString(digi[ID]->GetSettingValueFromMemory(para)));
          SendLogMsg(msg + "|OK.");
          sbChSettingsWrite->setStyleSheet("");
          UpdatePanelFromMemory();
          UpdateOtherPanels();
        }else{
          leChSettingsRead->setText("fail write value");
          SendLogMsg(msg + "|Fail.");
          sbChSettingsWrite->setStyleSheet("color:red;");
        }
      });

      leChSettingsWrite = new QLineEdit(ICTab);
      leChSettingsWrite->setFixedWidth(200);
      inquiryLayout->addWidget(leChSettingsWrite, rowID, 8);
      connect(leChSettingsWrite, &QLineEdit::textChanged, this, [=](){if( enableSignalSlot ) leChSettingsWrite->setStyleSheet("color: green;");});
      connect(leChSettingsWrite, &QLineEdit::returnPressed, this, [=](){
        if( !enableSignalSlot ) return;
        std::string value = leChSettingsWrite->text().toStdString();
        Reg para = PHA::CH::AllSettings[cbChSettings->currentIndex()];
        ID = cbIQDigi->currentIndex();
        int ch_index = cbIQCh->currentIndex();
        QString msg;
        msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(para.GetPara());
        msg += " = " + QString::number(sbChSettingsWrite->value());
        if( digi[ID]->WriteValue(para, value, ch_index)){
          leChSettingsRead->setText( QString::fromStdString(digi[ID]->GetSettingValueFromMemory(para)));
          SendLogMsg(msg + "|OK.");
          sbChSettingsWrite->setStyleSheet("");
          UpdatePanelFromMemory();
          UpdateOtherPanels();
        }else{
          leChSettingsRead->setText("fail write value");
          SendLogMsg(msg + "|Fail.");
          sbChSettingsWrite->setStyleSheet("color:red;");
        }
      });

      for( int i = 0 ; i < inquiryLayout->count(); i++) inquiryLayout->setColumnStretch(i, 1);

    }

    {//@============== Copy setting
      icBox2 = new QGroupBox("Copy Settings", ICTab);
      layout1->addWidget(icBox2);

      QGridLayout * cpLayout = new QGridLayout(icBox2);
      cpLayout->setAlignment(Qt::AlignCenter | Qt::AlignLeft);

      ///================
      QGroupBox * icBox2a = new QGroupBox("Copy From", icBox2);
      cpLayout->addWidget(icBox2a, 0, 0, 5, 1);

      QGridLayout * lo1 = new QGridLayout(icBox2a);

      cbCopyDigiFrom = new RComboBox(ICTab);
      for( int i = 0; i < nDigi; i++) cbCopyDigiFrom->addItem("Digi-" + QString::number(digi[i]->GetSerialNumber()), i);
      lo1->addWidget(cbCopyDigiFrom, 0, 0, 1, 8);
      connect(cbCopyDigiFrom, &RComboBox::currentIndexChanged, this, [=](int index){
        
        CheckRadioAndCheckedButtons();

        if( !CheckDigitizersCanCopy() ) {
          pbCopyChannel->setEnabled(false);
          return;
        }

        if( index == cbCopyDigiTo->currentIndex() ){
          pbCopyBoard->setEnabled(false);
          pbCopyDigi->setEnabled(false);
        }else{
          pbCopyBoard->setEnabled(true);
          pbCopyDigi->setEnabled(true);
        }
      });

      int rowID = 0;
      for( int i = 0; i < digi[0]->GetNChannels(); i++) {
        if( i % 8 == 0) rowID ++;
        rbCopyChFrom[i] = new QRadioButton("Ch-" + QString::number(i), ICTab);
        lo1->addWidget(rbCopyChFrom[i], rowID, i%8);
        connect(rbCopyChFrom[i], &QRadioButton::clicked, this, &DigiSettingsPanel::CheckRadioAndCheckedButtons);
      }

      ///================
      pbCopyChannel = new QPushButton("Copy Ch. Setting", ICTab);
      pbCopyChannel->setFixedSize(200, 50);
      cpLayout->addWidget(pbCopyChannel, 1, 1);
      pbCopyChannel->setEnabled(false);
      connect(pbCopyChannel, &QPushButton::clicked, this, [=](){ 
      
        int chFromIndex = -1;
        for( int i = 0 ; i < MaxNumberOfChannel ; i++){
          if( rbCopyChFrom[i]->isChecked() ){
            chFromIndex = i;
            break;
          }
        }
        if( chFromIndex < 0 ) return;

        for( int i = 0; i < MaxNumberOfChannel; i++){
          if( chkChTo[i]->isChecked() ) CopyChannelSettings(cbCopyDigiFrom->currentIndex(), chFromIndex, cbCopyDigiTo->currentIndex(), i );
        }
        SendLogMsg("------ done");
      });

      pbCopyBoard = new QPushButton("Copy Board Settings", ICTab);
      pbCopyBoard->setFixedSize(200, 50);
      cpLayout->addWidget(pbCopyBoard, 2, 1);
      pbCopyBoard->setEnabled(false);
      connect(pbCopyBoard, &QPushButton::clicked, this,  &DigiSettingsPanel::CopyBoardSettings);

      pbCopyDigi = new QPushButton("Copy Digitizer", ICTab);
      pbCopyDigi->setFixedSize(200, 50);
      cpLayout->addWidget(pbCopyDigi, 3, 1);
      pbCopyDigi->setEnabled(false);
      connect(pbCopyDigi, &QPushButton::clicked, this,  &DigiSettingsPanel::CopyWholeDigitizer);
      
      ///================
      QGroupBox * icBox2b = new QGroupBox("Copy To", icBox2);
      cpLayout->addWidget(icBox2b, 0, 2, 5, 1);

      QGridLayout * lo2 = new QGridLayout(icBox2b);

      cbCopyDigiTo = new RComboBox(ICTab);
      for( int i = 0; i < nDigi; i++) cbCopyDigiTo->addItem("Digi-" + QString::number(digi[i]->GetSerialNumber()), i);
      lo2->addWidget(cbCopyDigiTo, 0, 0, 1, 8);
      connect(cbCopyDigiTo, &RComboBox::currentIndexChanged, this, [=](int index){
        CheckRadioAndCheckedButtons();

        if( !CheckDigitizersCanCopy() ) {
          pbCopyChannel->setEnabled(false);
          return;
        }

        if( index == cbCopyDigiFrom->currentIndex() ){
          pbCopyBoard->setEnabled(false);
          pbCopyDigi->setEnabled(false);
        }else{
          pbCopyBoard->setEnabled(true);
          pbCopyDigi->setEnabled(true);
        }
      });

      rowID = 0;
      
      for( int i = 0; i < digi[0]->GetNChannels(); i++) {
        if( i % 8 == 0) rowID ++;
        chkChTo[i] = new QCheckBox("ch-" + QString::number(i), ICTab);
        lo2->addWidget(chkChTo[i], rowID, i%8);
        connect(chkChTo[i], &QCheckBox::clicked, this, &DigiSettingsPanel::CheckRadioAndCheckedButtons);
      }

      cpLayout->setColumnStretch(0, 4);
      cpLayout->setColumnStretch(1, 1);
      cpLayout->setColumnStretch(2, 4);
    }
  }

  connect(tabWidget, &QTabWidget::currentChanged, this, [=](int index){ 
    if( index < nDigi) {
      ID = index;
      UpdatePanelFromMemory(); 
    }else{
      ID = 0;
    }
  });

  for( ID = 0; ID < nDigi; ID ++) EnableControl();

  ID = 0;

  cbBdSettings->setCurrentText("ModelName");
  cbChSettings->setCurrentIndex(10);

  show();

  enableSignalSlot = true;
}

DigiSettingsPanel::~DigiSettingsPanel(){

  //printf("%s\n", __func__);

}


void DigiSettingsPanel::SetupPHAChannels(unsigned short digiID){

  //@.......... All Settings tab
  QWidget * tab_All = new QWidget(digiTab[digiID]); 
  //tab_All->setStyleSheet("background-color: #EEEEEE");
  chTabWidget[digiID]->addTab(tab_All, "All/Single Ch.");

  QGridLayout * allLayout = new QGridLayout(tab_All);
  allLayout->setAlignment(Qt::AlignTop);

  unsigned short ch = digi[digiID]->GetNChannels();

  {//*--------- Group 0
    box0[digiID] = new QGroupBox("Channel Selection", digiTab[digiID]);
    allLayout->addWidget(box0[digiID]);
    QGridLayout * layout0 = new QGridLayout(box0[digiID]);
    layout0->setAlignment(Qt::AlignLeft);

    QLabel * lbCh = new QLabel("Channel :", digiTab[digiID]);
    lbCh->setAlignment(Qt::AlignCenter | Qt::AlignRight);
    layout0->addWidget(lbCh, 0, 0);

    cbChPick[digiID] = new RComboBox(digiTab[digiID]);
    cbChPick[digiID]->addItem("All", -1);
    for( int i = 0; i < ch; i++) cbChPick[digiID]->addItem("Ch-" + QString::number(i), i);
    layout0->addWidget(cbChPick[digiID], 0, 1);
    connect(cbChPick[digiID], &RComboBox::currentIndexChanged, this, [=](){
      int index = cbChPick[ID]->currentData().toInt();
      if(index == -1) {
        UpdatePanelFromMemory();
        return;
      }else{
        enableSignalSlot = false;
        unsigned short ch = digi[digiID]->GetNChannels();
        //printf("index = %d, ch = %d\n", index, ch);
        FillComboBoxValueFromMemory(cbbOnOff[ID][ch], PHA::CH::ChannelEnable, index);
        FillSpinBoxValueFromMemory(spbDCOffset[ID][ch], PHA::CH::DC_Offset, index);
        FillSpinBoxValueFromMemory(spbThreshold[ID][ch], PHA::CH::TriggerThreshold, index);
        FillComboBoxValueFromMemory(cbbParity[ID][ch], PHA::CH::Polarity, index);
        FillSpinBoxValueFromMemory(spbRecordLength[ID][ch], PHA::CH::RecordLength, index);
        FillSpinBoxValueFromMemory(spbPreTrigger[ID][ch], PHA::CH::PreTrigger, index);

        FillComboBoxValueFromMemory(cbbWaveSource[ID][ch], PHA::CH::WaveDataSource, index);
        FillComboBoxValueFromMemory(cbbWaveRes[ID][ch], PHA::CH::WaveResolution, index);
        FillComboBoxValueFromMemory(cbbWaveSave[ID][ch], PHA::CH::WaveSaving, index);

        FillComboBoxValueFromMemory(cbbAnaProbe0[ID][ch], PHA::CH::WaveAnalogProbe0, index);
        FillComboBoxValueFromMemory(cbbAnaProbe1[ID][ch], PHA::CH::WaveAnalogProbe1, index);
        FillComboBoxValueFromMemory(cbbDigProbe0[ID][ch], PHA::CH::WaveDigitalProbe0, index);
        FillComboBoxValueFromMemory(cbbDigProbe1[ID][ch], PHA::CH::WaveDigitalProbe1, index);
        FillComboBoxValueFromMemory(cbbDigProbe2[ID][ch], PHA::CH::WaveDigitalProbe2, index);
        FillComboBoxValueFromMemory(cbbDigProbe3[ID][ch], PHA::CH::WaveDigitalProbe3, index);

        FillComboBoxValueFromMemory(cbbEventSelector[ID][ch], PHA::CH::EventSelector, index);
        FillComboBoxValueFromMemory(cbbWaveSelector[ID][ch], PHA::CH::WaveSelector, index);
        FillSpinBoxValueFromMemory(spbEnergySkimLow[ID][ch], PHA::CH::EnergySkimLowDiscriminator, index);
        FillSpinBoxValueFromMemory(spbEnergySkimHigh[ID][ch], PHA::CH::EnergySkimHighDiscriminator, index);

        FillComboBoxValueFromMemory(cbbEvtTrigger[ID][ch], PHA::CH::EventTriggerSource, index);
        FillComboBoxValueFromMemory(cbbWaveTrigger[ID][ch], PHA::CH::WaveTriggerSource, index);
        FillComboBoxValueFromMemory(cbbChVetoSrc[ID][ch], PHA::CH::ChannelVetoSource, index);
        FillComboBoxValueFromMemory(cbbCoinMask[ID][ch], PHA::CH::CoincidenceMask, index);
        FillComboBoxValueFromMemory(cbbAntiCoinMask[ID][ch], PHA::CH::AntiCoincidenceMask, index);
        FillSpinBoxValueFromMemory(spbCoinLength[ID][ch], PHA::CH::CoincidenceLength, index);
        FillSpinBoxValueFromMemory(spbADCVetoWidth[ID][ch], PHA::CH::ADCVetoWidth, index);

        unsigned long  mask = Utility::TenBase(digi[ID]->GetSettingValueFromMemory(PHA::CH::ChannelsTriggerMask, cbChPick[ID]->currentData().toInt()));
        leTriggerMask[ID][ch]->setText("0x" + QString::number(mask, 16).toUpper());

        //-------- PHA
        FillSpinBoxValueFromMemory(spbInputRiseTime[ID][ch], PHA::CH::TimeFilterRiseTime, index);
        FillSpinBoxValueFromMemory(spbTriggerGuard[ID][ch], PHA::CH::TimeFilterRetriggerGuard, index);
        FillComboBoxValueFromMemory(cbbLowFilter[ID][ch], PHA::CH::EnergyFilterLowFreqFilter, index);

        FillSpinBoxValueFromMemory(spbTrapRiseTime[ID][ch], PHA::CH::EnergyFilterRiseTime, index);
        FillSpinBoxValueFromMemory(spbTrapFlatTop[ID][ch], PHA::CH::EnergyFilterFlatTop, index);
        FillSpinBoxValueFromMemory(spbTrapPoleZero[ID][ch], PHA::CH::EnergyFilterPoleZero, index);

        FillSpinBoxValueFromMemory(spbPeaking[ID][ch], PHA::CH::EnergyFilterPeakingPosition, index);
        FillSpinBoxValueFromMemory(spbBaselineGuard[ID][ch], PHA::CH::EnergyFilterBaselineGuard, index);
        FillSpinBoxValueFromMemory(spbPileupGuard[ID][ch], PHA::CH::EnergyFilterPileUpGuard, index);

        FillComboBoxValueFromMemory(cbbBaselineAvg[ID][ch], PHA::CH::EnergyFilterBaselineAvg, index);
        FillComboBoxValueFromMemory(cbbPeakingAvg[ID][ch], PHA::CH::EnergyFilterPeakingAvg, index);
        FillSpinBoxValueFromMemory(spbFineGain[ID][ch], PHA::CH::EnergyFilterFineGain, index);

        enableSignalSlot = true;
      }
    });

  }

  int rowID = 0;
  {//*--------- Group 1
    box1[digiID] = new QGroupBox("Input Settings", digiTab[digiID]);
    allLayout->addWidget(box1[digiID]);
    QGridLayout * layout1 = new QGridLayout(box1[digiID]);

    rowID = 0;
    SetupComboBox(cbbOnOff[digiID][ch], PHA::CH::ChannelEnable, -1, true, "On/Off", layout1, rowID, 0);
    SetupComboBox(cbbWaveSource[digiID][ch], PHA::CH::WaveDataSource, -1, true, "Wave Data Source", layout1, rowID, 2);

    rowID ++;
    SetupComboBox(cbbWaveRes[digiID][ch], PHA::CH::WaveResolution, -1, true,  "Wave Resol.", layout1, rowID, 0);
    SetupComboBox(cbbWaveSave[digiID][ch], PHA::CH::WaveSaving, -1, true, "Wave Save", layout1, rowID, 2);

    rowID ++;
    SetupSpinBox(spbDCOffset[digiID][ch], PHA::CH::DC_Offset, -1, true, "DC Offset [%]", layout1, rowID, 0);
    SetupSpinBox(spbThreshold[digiID][ch], PHA::CH::TriggerThreshold, -1, true, "Threshold [LSB]", layout1, rowID, 2);

    rowID ++;
    SetupSpinBox(spbRecordLength[digiID][ch], PHA::CH::RecordLength, -1, true, "Record Length [ns]", layout1, rowID, 0);
    SetupSpinBox(spbPreTrigger[digiID][ch], PHA::CH::PreTrigger, -1, true, "Pre Trigger [ns]", layout1, rowID, 2);

    rowID ++;
    SetupComboBox(cbbParity[digiID][ch], PHA::CH::Polarity, -1, true, "Parity", layout1, rowID, 0);
    SetupComboBox(cbbLowFilter[digiID][ch], PHA::CH::EnergyFilterLowFreqFilter, -1, true, "Low Freq. Filter", layout1, rowID, 2);

    rowID ++;
    SetupSpinBox(spbInputRiseTime[digiID][ch], PHA::CH::TimeFilterRiseTime, -1, true, "Input Rise Time [ns]", layout1, rowID, 0);
    SetupSpinBox(spbTriggerGuard[digiID][ch], PHA::CH::TimeFilterRetriggerGuard, -1, true, "Trigger Guard [ns]", layout1, rowID, 2);

  }

  {//*--------- Group 2
    box3[digiID] = new QGroupBox("Trap. Settings", digiTab[digiID]);
    allLayout->addWidget(box3[digiID]);
    QGridLayout * layout3 = new QGridLayout(box3[digiID]);

    //------------------------------
    rowID = 0;    
    SetupSpinBox(spbTrapRiseTime[digiID][ch], PHA::CH::EnergyFilterRiseTime, -1, true, "Trap. Rise Time [ns]", layout3, rowID, 0);
    SetupSpinBox(spbTrapFlatTop[digiID][ch], PHA::CH::EnergyFilterFlatTop, -1, true, "Trap. Flat Top [ns]", layout3, rowID, 2);
    SetupSpinBox(spbTrapPoleZero[digiID][ch], PHA::CH::EnergyFilterPoleZero, -1, true, "Trap. Pole Zero [ns]", layout3, rowID, 4);
    
    //------------------------------
    rowID ++;
    SetupSpinBox(spbPeaking[digiID][ch], PHA::CH::EnergyFilterPeakingPosition, -1, true, "Peaking [%]", layout3, rowID, 0);
    SetupSpinBox(spbBaselineGuard[digiID][ch], PHA::CH::EnergyFilterBaselineGuard, -1, true, "Baseline Guard [ns]", layout3, rowID, 2);
    SetupSpinBox(spbPileupGuard[digiID][ch], PHA::CH::EnergyFilterPileUpGuard, -1, true, "Pile-up Guard [ns]", layout3, rowID, 4);
    
    //------------------------------
    rowID ++;
    SetupComboBox(cbbPeakingAvg[digiID][ch], PHA::CH::EnergyFilterPeakingAvg, -1, true, "Peak Avg", layout3, rowID, 0);
    SetupComboBox(cbbBaselineAvg[digiID][ch], PHA::CH::EnergyFilterBaselineAvg, -1, true, "Baseline Avg", layout3, rowID, 2);
    SetupSpinBox(spbFineGain[digiID][ch], PHA::CH::EnergyFilterFineGain, -1, true, "Fine Gain", layout3, rowID, 4);
    
  }

  {//*--------- Group 3
    box4[digiID] = new QGroupBox("Probe Settings", digiTab[digiID]);
    allLayout->addWidget(box4[digiID]);
    QGridLayout * layout4 = new QGridLayout(box4[digiID]);

    //------------------------------
    rowID = 0;
    SetupComboBox(cbbAnaProbe0[digiID][ch], PHA::CH::WaveAnalogProbe0, -1, true, "Analog Prob. 0", layout4, rowID, 0, 1, 2);
    SetupComboBox(cbbAnaProbe1[digiID][ch], PHA::CH::WaveAnalogProbe1, -1, true, "Analog Prob. 1", layout4, rowID, 3, 1, 2);        

    //------------------------------
    rowID ++;
    SetupComboBox(cbbDigProbe0[digiID][ch], PHA::CH::WaveDigitalProbe0, -1, true, "Digitial Prob. 0", layout4, rowID, 0, 1, 2);
    SetupComboBox(cbbDigProbe1[digiID][ch], PHA::CH::WaveDigitalProbe1, -1, true, "Digitial Prob. 1", layout4, rowID, 3, 1, 2);

    //------------------------------
    rowID ++;
    SetupComboBox(cbbDigProbe2[digiID][ch], PHA::CH::WaveDigitalProbe2, -1, true, "Digitial Prob. 2", layout4, rowID, 0, 1, 2);
    SetupComboBox(cbbDigProbe3[digiID][ch], PHA::CH::WaveDigitalProbe3, -1, true, "Digitial Prob. 3", layout4, rowID, 3, 1, 2);
    
  }

  {//*--------- Group 4
    box5[digiID] = new QGroupBox("Trigger Settings", digiTab[digiID]);
    allLayout->addWidget(box5[digiID]);
    QGridLayout * layout5 = new QGridLayout(box5[digiID]);

    //------------------------------
    rowID = 0;
    SetupComboBox(cbbEvtTrigger[digiID][ch], PHA::CH::EventTriggerSource, -1, true, "Event Trig. Source", layout5, rowID, 0);
    SetupComboBox(cbbWaveTrigger[digiID][ch], PHA::CH::WaveTriggerSource, -1, true, "Wave Trig. Source", layout5, rowID, 2);

    //------------------------------
    rowID ++;
    SetupComboBox(cbbChVetoSrc[digiID][ch], PHA::CH::ChannelVetoSource, -1, true, "Veto Source", layout5, rowID, 0);

    QLabel * lbTrgMsk = new QLabel("Trigger Mask");
    lbTrgMsk->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    layout5->addWidget(lbTrgMsk, rowID, 2);
    leTriggerMask[digiID][ch] = new QLineEdit(this);
    leTriggerMask[digiID][ch]->setToolTip("Both Hex or Dec is OK.");
    layout5->addWidget(leTriggerMask[digiID][ch], rowID, 3); 

    connect(leTriggerMask[digiID][ch], &QLineEdit::textChanged, this, [=](){
      if( !enableSignalSlot ) return; 
      leTriggerMask[digiID][ch]->setStyleSheet("color:blue;");
    });

    connect(leTriggerMask[digiID][ch], &QLineEdit::returnPressed, this, [=](){
      if( !enableSignalSlot ) return; 
      int index = cbChPick[ID]->currentData().toInt();

      QString SixteenBaseValue = "0x" + QString::number(Utility::TenBase(leTriggerMask[ID][ch]->text().toStdString()), 16).toUpper();
      leTriggerMask[ID][ch]->setText(SixteenBaseValue);

      QString msg;
      msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(PHA::CH::ChannelsTriggerMask.GetPara()) ;
      msg += ",CH:" + (index == -1 ? "All" : QString::number(index));
      msg += " = " + SixteenBaseValue;

      if( digi[ID]->WriteValue(PHA::CH::ChannelsTriggerMask, SixteenBaseValue.toStdString(), index)){
        SendLogMsg(msg + "|OK.");
        leTriggerMask[ID][ch]->setStyleSheet("");
        UpdatePanelFromMemory();
        UpdateOtherPanels();
      }else{
        SendLogMsg(msg + "|Fail.");
        leTriggerMask[ID][ch]->setStyleSheet("color:red;");
      }
    });

    //------------------------------
    rowID ++;
    SetupComboBox(cbbCoinMask[digiID][ch], PHA::CH::CoincidenceMask, -1, true, "Coin. Mask", layout5, rowID, 0);
    SetupComboBox(cbbAntiCoinMask[digiID][ch], PHA::CH::AntiCoincidenceMask, -1, true, "Anti-Coin. Mask", layout5, rowID, 2);

    //------------------------------
    rowID ++;
    SetupSpinBox(spbCoinLength[digiID][ch], PHA::CH::CoincidenceLength, -1, true, "Coin. Length [ns]", layout5, rowID, 0);
    SetupSpinBox(spbADCVetoWidth[digiID][ch], PHA::CH::ADCVetoWidth, -1, true, "ADC Veto Length [ns]", layout5, rowID, 2);

    for( int i = 0; i < layout5->columnCount(); i++) layout5->setColumnStretch(i, 1);

  }

  {//*--------- Group 5
    box6[digiID] = new QGroupBox("Other Settings", digiTab[digiID]);
    allLayout->addWidget(box6[digiID]);
    QGridLayout * layout6 = new QGridLayout(box6[digiID]);

    //------------------------------
    rowID = 0 ;
    SetupComboBox(cbbEventSelector[digiID][ch], PHA::CH::EventSelector, -1, true, "Event Selector", layout6, rowID, 0);
    SetupComboBox(cbbWaveSelector[digiID][ch], PHA::CH::WaveSelector, -1, true, "Wave Selector", layout6, rowID, 2);

    //------------------------------
    rowID ++;
    SetupSpinBox(spbEnergySkimLow[digiID][ch], PHA::CH::EnergySkimLowDiscriminator, -1, true, "Energy Skim Low", layout6, rowID, 0);
    SetupSpinBox(spbEnergySkimHigh[digiID][ch], PHA::CH::EnergySkimHighDiscriminator, -1, true, "Energy Skim High", layout6, rowID, 2);
  }


  
  {//@============== input  tab
    inputTab[digiID] = new QTabWidget(digiTab[digiID]);
    chTabWidget[digiID]->addTab(inputTab[digiID], "Input");

    SetupComboBoxTab(cbbOnOff, PHA::CH::ChannelEnable, "On/Off", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbWaveSource, PHA::CH::WaveDataSource, "Wave Data Dource", inputTab[digiID], digiID, digi[digiID]->GetNChannels(), 2);
    SetupComboBoxTab(cbbWaveRes, PHA::CH::WaveResolution,  "Wave Resol.", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbWaveSave, PHA::CH::WaveSaving, "Wave Save", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbDCOffset, PHA::CH::DC_Offset, "DC Offset [%]", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbThreshold, PHA::CH::TriggerThreshold, "Threshold [LSB]", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbRecordLength, PHA::CH::RecordLength, "Record Length [ns]", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbPreTrigger, PHA::CH::PreTrigger, "PreTrigger [ns]", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbParity, PHA::CH::Polarity, "Parity", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    
    SetupSpinBoxTab(spbInputRiseTime, PHA::CH::TimeFilterRiseTime, "Input Rise Time [ns]", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbTriggerGuard, PHA::CH::TimeFilterRetriggerGuard, "Trigger Guard [ns]", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbLowFilter, PHA::CH::EnergyFilterLowFreqFilter, "Low Freq. Filter", inputTab[digiID], digiID, digi[digiID]->GetNChannels());

    for( int ch = 0; ch < digi[digiID]->GetNChannels(); ch++){
      //Set color of some combox
      cbbOnOff[digiID][ch]->setItemData(1, QBrush(orangeColor), Qt::ForegroundRole);
      connect(cbbOnOff[digiID][ch], &RComboBox::currentIndexChanged, this, [=](int index){ cbbOnOff[ID][ch]->setStyleSheet(index == 1 ? "color : orange;" : "");});
      cbbParity[digiID][ch]->setItemData(1, QBrush(orangeColor), Qt::ForegroundRole);
      connect(cbbParity[digiID][ch], &RComboBox::currentIndexChanged, this, [=](int index){ cbbParity[ID][ch]->setStyleSheet(index == 1 ?  "color : orange;" : "");});
      cbbLowFilter[digiID][ch]->setItemData(1, QBrush(orangeColor), Qt::ForegroundRole);
      connect(cbbLowFilter[digiID][ch], &RComboBox::currentIndexChanged, this, [=](int index){ cbbLowFilter[ID][ch]->setStyleSheet(index == 1 ? "color : orange;": "");});
    }

  }
  
  {//@============== Trap  tab
    trapTab[digiID] = new QTabWidget(digiTab[digiID]);
    chTabWidget[digiID]->addTab(trapTab[digiID], "Trapezoid");

    SetupSpinBoxTab(spbTrapRiseTime, PHA::CH::EnergyFilterRiseTime, "Trap. Rise Time [ns]", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbTrapFlatTop, PHA::CH::EnergyFilterFlatTop, "Trap. Flat Top [ns]", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbTrapPoleZero, PHA::CH::EnergyFilterPoleZero, "Trap. Pole Zero [ns]", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbPeaking, PHA::CH::EnergyFilterPeakingPosition, "Peaking [%]", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbPeakingAvg, PHA::CH::EnergyFilterPeakingAvg, "Peak Avg.", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbBaselineAvg, PHA::CH::EnergyFilterBaselineAvg, "Baseline Avg.", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbFineGain, PHA::CH::EnergyFilterFineGain, "Fine Gain", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbBaselineGuard, PHA::CH::EnergyFilterBaselineGuard, "Baseline Guard [ns]", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbPileupGuard, PHA::CH::EnergyFilterPileUpGuard, "Pile-up Guard [ns]", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
  }

  {//@============== Probe  tab
    probeTab[digiID] = new QTabWidget(digiTab[digiID]);
    chTabWidget[digiID]->addTab(probeTab[digiID], "Probe");

    SetupComboBoxTab(cbbAnaProbe0, PHA::CH::WaveAnalogProbe0, "Analog Prob. 0", probeTab[digiID], digiID, digi[digiID]->GetNChannels(), 4);
    SetupComboBoxTab(cbbAnaProbe1, PHA::CH::WaveAnalogProbe1, "Analog Prob. 1", probeTab[digiID], digiID, digi[digiID]->GetNChannels(), 4);
    SetupComboBoxTab(cbbDigProbe0, PHA::CH::WaveDigitalProbe0, "Digital Prob. 0", probeTab[digiID], digiID, digi[digiID]->GetNChannels(), 4);
    SetupComboBoxTab(cbbDigProbe1, PHA::CH::WaveDigitalProbe1, "Digital Prob. 1", probeTab[digiID], digiID, digi[digiID]->GetNChannels(), 4);
    SetupComboBoxTab(cbbDigProbe2, PHA::CH::WaveDigitalProbe2, "Digital Prob. 2", probeTab[digiID], digiID, digi[digiID]->GetNChannels(), 4);
    SetupComboBoxTab(cbbDigProbe3, PHA::CH::WaveDigitalProbe3, "Digital Prob. 3", probeTab[digiID], digiID, digi[digiID]->GetNChannels(), 4);
  }

  {//@============== Other  tab
    otherTab[digiID] = new QTabWidget(digiTab[digiID]);
    chTabWidget[digiID]->addTab(otherTab[digiID], "Others");

    SetupComboBoxTab(cbbEventSelector, PHA::CH::EventSelector, "Event Selector", otherTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbWaveSelector, PHA::CH::WaveSelector, "Wave Selector", otherTab[digiID], digiID, digi[digiID]->GetNChannels(), 2 );
    SetupSpinBoxTab(spbEnergySkimLow, PHA::CH::EnergySkimLowDiscriminator, "Energy Skim Low", otherTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbEnergySkimHigh, PHA::CH::EnergySkimHighDiscriminator, "Energy Skim High", otherTab[digiID], digiID, digi[digiID]->GetNChannels());
  }

  {//@============== Trigger  tab
    triggerTab[digiID] = new QTabWidget(digiTab[digiID]);
    chTabWidget[digiID]->addTab(triggerTab[digiID], "Trigger");

    SetupComboBoxTab(cbbEvtTrigger, PHA::CH::EventTriggerSource, "Event Trig. Source", triggerTab[digiID], digiID, digi[digiID]->GetNChannels(), 2);
    SetupComboBoxTab(cbbWaveTrigger, PHA::CH::WaveTriggerSource, "Wave Trig. Source", triggerTab[digiID], digiID, digi[digiID]->GetNChannels(), 2);
    SetupComboBoxTab(cbbChVetoSrc, PHA::CH::ChannelVetoSource, "Veto Source", triggerTab[digiID], digiID, digi[digiID]->GetNChannels(), 2);
    SetupComboBoxTab(cbbCoinMask, PHA::CH::CoincidenceMask, "Coin. Mask", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbAntiCoinMask, PHA::CH::AntiCoincidenceMask, "Anti-Coin. Mask", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbCoinLength, PHA::CH::CoincidenceLength, "Coin. Length [ns]", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbADCVetoWidth, PHA::CH::ADCVetoWidth, "ADC Veto Length [ns]", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
  }

  for( int ch = 0; ch < digi[ID]->GetNChannels() + 1; ch++) {
    //----- SyncBox
    connect(cbbOnOff[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbOnOff, ch);});
    connect(spbDCOffset[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbDCOffset, ch);});
    connect(spbThreshold[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbThreshold, ch);});
    connect(cbbParity[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbParity, ch);});
    connect(spbRecordLength[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbRecordLength, ch);});
    connect(spbPreTrigger[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbPreTrigger, ch);});
    
    connect(cbbWaveSource[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveSource, ch);});
    connect(cbbWaveRes[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveRes, ch);});
    connect(cbbWaveSave[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveSave, ch);});

    connect(cbbAnaProbe0[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbAnaProbe0, ch);});
    connect(cbbAnaProbe1[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbAnaProbe1, ch);});
    connect(cbbDigProbe0[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe0, ch);});
    connect(cbbDigProbe1[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe1, ch);});
    connect(cbbDigProbe2[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe2, ch);});
    connect(cbbDigProbe3[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe3, ch);});

    connect(cbbEventSelector[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbEventSelector, ch);});
    connect(cbbWaveSelector[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveSelector, ch);});
    connect(spbEnergySkimLow[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbEnergySkimLow, ch);});
    connect(spbEnergySkimHigh[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbEnergySkimHigh, ch);});

    connect(cbbEvtTrigger[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbEvtTrigger, ch);});
    connect(cbbWaveTrigger[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveTrigger, ch);});
    connect(cbbChVetoSrc[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbChVetoSrc, ch);});
    connect(cbbCoinMask[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbCoinMask, ch);});
    connect(cbbAntiCoinMask[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbAntiCoinMask, ch);});
    connect(spbCoinLength[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbCoinLength, ch);});
    connect(spbADCVetoWidth[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbADCVetoWidth, ch);});

    connect(spbInputRiseTime[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbInputRiseTime, ch);});
    connect(spbTriggerGuard[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbTriggerGuard, ch);});
    connect(cbbLowFilter[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbLowFilter, ch);});
    
    connect(spbTrapRiseTime[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbTrapRiseTime, ch);});
    connect(spbTrapFlatTop[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbTrapFlatTop, ch);});
    connect(spbTrapPoleZero[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbTrapPoleZero, ch);});
 
    connect(spbPeaking[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbPeaking, ch);});
    connect(spbBaselineGuard[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbBaselineGuard, ch);});
    connect(spbPileupGuard[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbPileupGuard, ch);});
 
    connect(cbbBaselineAvg[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbBaselineAvg, ch);});
    connect(cbbPeakingAvg[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbPeakingAvg, ch);});
    connect(spbFineGain[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbFineGain, ch);});
    spbFineGain[digiID][ch]->setSingleStep(0.001);
    spbFineGain[digiID][ch]->setDecimals(3);

  }

  
}

void DigiSettingsPanel::SetupPSDChannels(unsigned short digiID){

  //@.......... All Settings tab
  QWidget * tab_All = new QWidget(digiTab[digiID]); 
  //tab_All->setStyleSheet("background-color: #EEEEEE");
  chTabWidget[digiID]->addTab(tab_All, "All/Single Ch.");

  QGridLayout * allLayout = new QGridLayout(tab_All);
  allLayout->setAlignment(Qt::AlignTop);

  unsigned short ch = digi[digiID]->GetNChannels();

  {//*--------- Group 0
    box0[digiID] = new QGroupBox("Channel Selection", digiTab[digiID]);
    allLayout->addWidget(box0[digiID]);
    QGridLayout * layout0 = new QGridLayout(box0[digiID]);
    layout0->setAlignment(Qt::AlignLeft);

    QLabel * lbCh = new QLabel("Channel :", digiTab[digiID]);
    lbCh->setAlignment(Qt::AlignCenter | Qt::AlignRight);
    layout0->addWidget(lbCh, 0, 0);

    cbChPick[digiID] = new RComboBox(digiTab[digiID]);
    cbChPick[digiID]->addItem("All", -1);
    for( int i = 0; i < ch; i++) cbChPick[digiID]->addItem("Ch-" + QString::number(i), i);
    layout0->addWidget(cbChPick[digiID], 0, 1);
    connect(cbChPick[digiID], &RComboBox::currentIndexChanged, this, [=](){
      int index = cbChPick[ID]->currentData().toInt();
      if(index == -1) {
        UpdatePanelFromMemory();
        return;
      }else{
        enableSignalSlot = false;
        unsigned short ch = digi[digiID]->GetNChannels();
        //printf("index = %d, ch = %d\n", index, ch);
        FillComboBoxValueFromMemory(cbbOnOff[ID][ch], PHA::CH::ChannelEnable, index);
        FillSpinBoxValueFromMemory(spbDCOffset[ID][ch], PHA::CH::DC_Offset, index);
        FillSpinBoxValueFromMemory(spbThreshold[ID][ch], PHA::CH::TriggerThreshold, index);
        FillComboBoxValueFromMemory(cbbParity[ID][ch], PHA::CH::Polarity, index);
        FillSpinBoxValueFromMemory(spbRecordLength[ID][ch], PHA::CH::RecordLength, index);
        FillSpinBoxValueFromMemory(spbPreTrigger[ID][ch], PHA::CH::PreTrigger, index);

        FillComboBoxValueFromMemory(cbbWaveSource[ID][ch], PHA::CH::WaveDataSource, index);
        FillComboBoxValueFromMemory(cbbWaveRes[ID][ch], PHA::CH::WaveResolution, index);
        FillComboBoxValueFromMemory(cbbWaveSave[ID][ch], PHA::CH::WaveSaving, index);

        FillComboBoxValueFromMemory(cbbAnaProbe0[ID][ch], PHA::CH::WaveAnalogProbe0, index);
        FillComboBoxValueFromMemory(cbbAnaProbe1[ID][ch], PHA::CH::WaveAnalogProbe1, index);
        FillComboBoxValueFromMemory(cbbDigProbe0[ID][ch], PHA::CH::WaveDigitalProbe0, index);
        FillComboBoxValueFromMemory(cbbDigProbe1[ID][ch], PHA::CH::WaveDigitalProbe1, index);
        FillComboBoxValueFromMemory(cbbDigProbe2[ID][ch], PHA::CH::WaveDigitalProbe2, index);
        FillComboBoxValueFromMemory(cbbDigProbe3[ID][ch], PHA::CH::WaveDigitalProbe3, index);

        FillComboBoxValueFromMemory(cbbEventSelector[ID][ch], PHA::CH::EventSelector, index);
        FillComboBoxValueFromMemory(cbbWaveSelector[ID][ch], PHA::CH::WaveSelector, index);
        FillSpinBoxValueFromMemory(spbEnergySkimLow[ID][ch], PHA::CH::EnergySkimLowDiscriminator, index);
        FillSpinBoxValueFromMemory(spbEnergySkimHigh[ID][ch], PHA::CH::EnergySkimHighDiscriminator, index);

        FillComboBoxValueFromMemory(cbbEvtTrigger[ID][ch], PHA::CH::EventTriggerSource, index);
        FillComboBoxValueFromMemory(cbbWaveTrigger[ID][ch], PHA::CH::WaveTriggerSource, index);
        FillComboBoxValueFromMemory(cbbChVetoSrc[ID][ch], PHA::CH::ChannelVetoSource, index);
        FillComboBoxValueFromMemory(cbbCoinMask[ID][ch], PHA::CH::CoincidenceMask, index);
        FillComboBoxValueFromMemory(cbbAntiCoinMask[ID][ch], PHA::CH::AntiCoincidenceMask, index);
        FillSpinBoxValueFromMemory(spbCoinLength[ID][ch], PHA::CH::CoincidenceLength, index);
        FillSpinBoxValueFromMemory(spbADCVetoWidth[ID][ch], PHA::CH::ADCVetoWidth, index);

        unsigned long  mask = Utility::TenBase(digi[ID]->GetSettingValueFromMemory(PHA::CH::ChannelsTriggerMask, cbChPick[ID]->currentData().toInt()));
        leTriggerMask[ID][ch]->setText("0x" + QString::number(mask, 16).toUpper());

        //-------- PSD
        FillComboBoxValueFromMemory(cbbADCInputBaselineAvg[ID][ch], PSD::CH::ADCInputBaselineAvg, index);
        FillSpinBoxValueFromMemory(spbAbsBaseline[ID][ch], PSD::CH::AbsoluteBaseline, index);
        FillSpinBoxValueFromMemory(spbADCInputBaselineGuard[ID][ch], PSD::CH::ADCInputBaselineGuard, index);

        FillComboBoxValueFromMemory(cbbTriggerFilter[ID][ch], PSD::CH::TriggerFilterSelection, index);
        FillComboBoxValueFromMemory(cbbTriggerHysteresis[ID][ch], PSD::CH::TriggerHysteresis, index);
        FillSpinBoxValueFromMemory(spbCFDDelay[ID][ch], PSD::CH::CFDDelay, index);
        FillSpinBoxValueFromMemory(spbCFDFraction[ID][ch], PSD::CH::CFDFraction, index);
        FillComboBoxValueFromMemory(cbbSmoothingFactor[ID][ch], PSD::CH::SmoothingFactor, index);
        FillComboBoxValueFromMemory(cbbChargeSmooting[ID][ch], PSD::CH::ChargeSmoothing, index);
        FillComboBoxValueFromMemory(cbbTimeFilterSmoothing[ID][ch], PSD::CH::TimeFilterSmoothing, index);
        FillSpinBoxValueFromMemory(spbTimeFilterReTriggerGuard[ID][ch], PSD::CH::TimeFilterRetriggerGuard, index);
        FillSpinBoxValueFromMemory(spbPileupGap[ID][ch], PSD::CH::PileupGap, index);

        FillSpinBoxValueFromMemory(spbGateLong[ID][ch], PSD::CH::GateLongLength, index);
        FillSpinBoxValueFromMemory(spbGateShort[ID][ch], PSD::CH::GateShortLength, index);
        FillSpinBoxValueFromMemory(spbGateOffset[ID][ch], PSD::CH::GateOffset, index);
        FillSpinBoxValueFromMemory(spbLongChargeIntergratorPedestal[ID][ch], PSD::CH::LongChargeIntegratorPedestal, index);
        FillSpinBoxValueFromMemory(spbShortChargeIntergratorPedestal[ID][ch], PSD::CH::ShortChargeIntegratorPedestal, index);
        FillComboBoxValueFromMemory(cbbEnergyGain[ID][ch], PSD::CH::EnergyGain, index);

        FillSpinBoxValueFromMemory(spbNeutronThreshold[ID][ch], PSD::CH::NeutronThreshold, index);
        FillComboBoxValueFromMemory(cbbEventNeutronReject[ID][ch], PSD::CH::EventNeutronReject, index);
        FillComboBoxValueFromMemory(cbbWaveNeutronReject[ID][ch], PSD::CH::WaveNeutronReject, index);

        enableSignalSlot = true;
      }
    });

  }

  int rowID = 0;
  {//*--------- Group 1
    box1[digiID] = new QGroupBox("Input Settings", digiTab[digiID]);
    allLayout->addWidget(box1[digiID]);
    QGridLayout * layout1 = new QGridLayout(box1[digiID]);

    rowID = 0;
    SetupComboBox(cbbOnOff[digiID][ch], PHA::CH::ChannelEnable, -1, true, "On/Off", layout1, rowID, 0);
    SetupComboBox(cbbWaveSource[digiID][ch], PHA::CH::WaveDataSource, -1, true, "Wave Data Source", layout1, rowID, 2);

    rowID ++;
    SetupComboBox(cbbWaveRes[digiID][ch], PHA::CH::WaveResolution, -1, true,  "Wave Resol.", layout1, rowID, 0);
    SetupComboBox(cbbWaveSave[digiID][ch], PHA::CH::WaveSaving, -1, true, "Wave Save", layout1, rowID, 2);

    rowID ++;
    SetupSpinBox(spbDCOffset[digiID][ch], PHA::CH::DC_Offset, -1, true, "DC Offset [%]", layout1, rowID, 0);
    SetupSpinBox(spbThreshold[digiID][ch], PHA::CH::TriggerThreshold, -1, true, "Threshold [LSB]", layout1, rowID, 2);

    rowID ++;
    SetupSpinBox(spbRecordLength[digiID][ch], PHA::CH::RecordLength, -1, true, "Record Length [ns]", layout1, rowID, 0);
    SetupSpinBox(spbPreTrigger[digiID][ch], PHA::CH::PreTrigger, -1, true, "Pre Trigger [ns]", layout1, rowID, 2);

    rowID ++;
    SetupComboBox(cbbParity[digiID][ch], PHA::CH::Polarity, -1, true, "Parity", layout1, rowID, 0);
    SetupComboBox(cbbADCInputBaselineAvg[digiID][ch], PSD::CH::ADCInputBaselineAvg, -1, true, "ADC Input BL Avg.", layout1, rowID, 2);

    rowID ++;
    SetupSpinBox(spbAbsBaseline[digiID][ch], PSD::CH::AbsoluteBaseline, -1, true, "Abs. Baseline", layout1, rowID, 0);
    SetupSpinBox(spbADCInputBaselineGuard[digiID][ch], PSD::CH::ADCInputBaselineGuard, -1, true, "ADC Input BL  Gaurd [ns]", layout1, rowID, 2);

  }

  {//*--------- Group 2
    box3[digiID] = new QGroupBox("Gate Settings", digiTab[digiID]);
    allLayout->addWidget(box3[digiID]);
    QGridLayout * layout3 = new QGridLayout(box3[digiID]);

    //------------------------------
    rowID = 0;    
    SetupSpinBox(spbGateLong[digiID][ch], PSD::CH::GateLongLength, -1, true, "Long Gate [ns]", layout3, rowID, 0);
    SetupSpinBox(spbGateShort[digiID][ch], PSD::CH::GateShortLength, -1, true, "Short Gate [ns]", layout3, rowID, 2);

    rowID ++;
    SetupSpinBox(spbGateOffset[digiID][ch], PSD::CH::GateOffset, -1, true, "Gate Offset [ns]", layout3, rowID, 0);
    SetupComboBox(cbbEnergyGain[digiID][ch], PSD::CH::EnergyGain, -1, true, "Energy Gain", layout3, rowID, 2);

    rowID ++;
    SetupSpinBox(spbLongChargeIntergratorPedestal[digiID][ch], PSD::CH::LongChargeIntegratorPedestal, -1, true, "Long Ped.", layout3, rowID, 0);
    SetupSpinBox(spbShortChargeIntergratorPedestal[digiID][ch], PSD::CH::ShortChargeIntegratorPedestal, -1, true, "Short Ped.", layout3, rowID, 2);
    
  }

  {//*--------- Group 4
    box4[digiID] = new QGroupBox("Probe Settings", digiTab[digiID]);
    allLayout->addWidget(box4[digiID]);
    QGridLayout * layout4 = new QGridLayout(box4[digiID]);

    //------------------------------
    rowID = 0;
    SetupComboBox(cbbAnaProbe0[digiID][ch], PSD::CH::WaveAnalogProbe0, -1, true, "Analog Prob. 0", layout4, rowID, 0, 1, 2);
    SetupComboBox(cbbAnaProbe1[digiID][ch], PSD::CH::WaveAnalogProbe1, -1, true, "Analog Prob. 1", layout4, rowID, 3, 1, 2);        

    //------------------------------
    rowID ++;
    SetupComboBox(cbbDigProbe0[digiID][ch], PSD::CH::WaveDigitalProbe0, -1, true, "Digitial Prob. 0", layout4, rowID, 0, 1, 2);
    SetupComboBox(cbbDigProbe1[digiID][ch], PSD::CH::WaveDigitalProbe1, -1, true, "Digitial Prob. 1", layout4, rowID, 3, 1, 2);

    //------------------------------
    rowID ++;
    SetupComboBox(cbbDigProbe2[digiID][ch], PSD::CH::WaveDigitalProbe2, -1, true, "Digitial Prob. 2", layout4, rowID, 0, 1, 2);
    SetupComboBox(cbbDigProbe3[digiID][ch], PSD::CH::WaveDigitalProbe3, -1, true, "Digitial Prob. 3", layout4, rowID, 3, 1, 2);
    
  }

  {//*--------- Group 5
    box5[digiID] = new QGroupBox("Trigger Settings", digiTab[digiID]);
    allLayout->addWidget(box5[digiID]);
    QGridLayout * layout5 = new QGridLayout(box5[digiID]);

    //------------------------------
    rowID = 0;
    SetupComboBox(cbbEvtTrigger[digiID][ch], PHA::CH::EventTriggerSource, -1, true, "Event Trig. Source", layout5, rowID, 0);
    SetupComboBox(cbbWaveTrigger[digiID][ch], PHA::CH::WaveTriggerSource, -1, true, "Wave Trig. Source", layout5, rowID, 2);

    //------------------------------
    rowID ++;
    SetupComboBox(cbbChVetoSrc[digiID][ch], PHA::CH::ChannelVetoSource, -1, true, "Veto Source", layout5, rowID, 0);

    QLabel * lbTrgMsk = new QLabel("Trigger Mask");
    lbTrgMsk->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    layout5->addWidget(lbTrgMsk, rowID, 2);
    leTriggerMask[digiID][ch] = new QLineEdit(this);
    leTriggerMask[digiID][ch]->setToolTip("Both Hex or Dec is OK.");
    layout5->addWidget(leTriggerMask[digiID][ch], rowID, 3); 

    connect(leTriggerMask[digiID][ch], &QLineEdit::textChanged, this, [=](){
      if( !enableSignalSlot ) return; 
      leTriggerMask[digiID][ch]->setStyleSheet("color:blue;");
    });

    connect(leTriggerMask[digiID][ch], &QLineEdit::returnPressed, this, [=](){
      if( !enableSignalSlot ) return; 
      int index = cbChPick[ID]->currentData().toInt();

      QString SixteenBaseValue = "0x" + QString::number(Utility::TenBase(leTriggerMask[ID][ch]->text().toStdString()), 16).toUpper();
      leTriggerMask[ID][ch]->setText(SixteenBaseValue);

      QString msg;
      msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(PHA::CH::ChannelsTriggerMask.GetPara()) ;
      msg += ",CH:" + (index == -1 ? "All" : QString::number(index));
      msg += " = " + SixteenBaseValue;

      if( digi[ID]->WriteValue(PHA::CH::ChannelsTriggerMask, SixteenBaseValue.toStdString(), index)){
        SendLogMsg(msg + "|OK.");
        leTriggerMask[ID][ch]->setStyleSheet("");
        UpdatePanelFromMemory();
        UpdateOtherPanels();
      }else{
        SendLogMsg(msg + "|Fail.");
        leTriggerMask[ID][ch]->setStyleSheet("color:red;");
      }
    });

    //------------------------------
    rowID ++;
    SetupComboBox(cbbCoinMask[digiID][ch], PHA::CH::CoincidenceMask, -1, true, "Coin. Mask", layout5, rowID, 0);
    SetupComboBox(cbbAntiCoinMask[digiID][ch], PHA::CH::AntiCoincidenceMask, -1, true, "Anti-Coin. Mask", layout5, rowID, 2);

    //------------------------------
    rowID ++;
    SetupSpinBox(spbCoinLength[digiID][ch], PHA::CH::CoincidenceLength, -1, true, "Coin. Length [ns]", layout5, rowID, 0);
    SetupSpinBox(spbADCVetoWidth[digiID][ch], PHA::CH::ADCVetoWidth, -1, true, "ADC Veto Length [ns]", layout5, rowID, 2);

    //------------------------------
    rowID ++;    
    SetupComboBox(cbbTriggerFilter[digiID][ch], PSD::CH::TriggerFilterSelection, -1, true, "Trig. Filter", layout5, rowID, 0);
    SetupComboBox(cbbTriggerHysteresis[digiID][ch], PSD::CH::TriggerHysteresis, -1, true, "Trig. Hysteresis", layout5, rowID, 2);

    rowID ++;
    SetupSpinBox(spbPileupGap[digiID][ch], PSD::CH::PileupGap, -1, true, "Pile-Up Gap [ns]", layout5, rowID, 0);
    SetupSpinBox(spbTimeFilterReTriggerGuard[digiID][ch], PSD::CH::TimeFilterRetriggerGuard,-1, true, "Time Filter Re.Trg Guard [ns]", layout5, rowID, 2);
    
    rowID ++;
    SetupSpinBox(spbCFDDelay[digiID][ch], PSD::CH::CFDDelay, -1, true, "CFD Delay [ns]", layout5, rowID, 0);
    SetupSpinBox(spbCFDFraction[digiID][ch], PSD::CH::CFDFraction, -1, true, "CFD Frac. [%]", layout5, rowID, 2);

    rowID ++;
    SetupComboBox(cbbSmoothingFactor[digiID][ch], PSD::CH::SmoothingFactor, -1, true, "Smoothing Fact.", layout5, rowID, 0);
    SetupComboBox(cbbChargeSmooting[digiID][ch], PSD::CH::ChargeSmoothing, -1, true, "Charge Smoothing", layout5, rowID, 2);

    rowID ++;
    SetupComboBox(cbbTimeFilterSmoothing[digiID][ch], PSD::CH::TimeFilterSmoothing, -1, true, "Time Filter Smoothing", layout5, rowID, 0);
    
    for( int i = 0; i < layout5->columnCount(); i++) layout5->setColumnStretch(i, 1);

  }

  {//*--------- Group 6
    box6[digiID] = new QGroupBox("Other Settings", digiTab[digiID]);
    allLayout->addWidget(box6[digiID]);
    QGridLayout * layout6 = new QGridLayout(box6[digiID]);

    //------------------------------
    rowID = 0 ;
    SetupComboBox(cbbEventSelector[digiID][ch], PHA::CH::EventSelector, -1, true, "Event Selector", layout6, rowID, 0);
    SetupComboBox(cbbWaveSelector[digiID][ch], PHA::CH::WaveSelector, -1, true, "Wave Selector", layout6, rowID, 2);

    //------------------------------
    rowID ++;
    SetupSpinBox(spbEnergySkimLow[digiID][ch], PHA::CH::EnergySkimLowDiscriminator, -1, true, "Energy Skim Low", layout6, rowID, 0);
    SetupSpinBox(spbEnergySkimHigh[digiID][ch], PHA::CH::EnergySkimHighDiscriminator, -1, true, "Energy Skim High", layout6, rowID, 2);

    //------------------------------
    rowID ++;
    SetupComboBox(cbbEventNeutronReject[digiID][ch], PSD::CH::EventNeutronReject, -1, true, "Event Neutron Rej.", layout6, rowID, 0);
    SetupComboBox(cbbWaveNeutronReject[digiID][ch], PSD::CH::WaveNeutronReject, -1, true, "Wave Neutron Rej.", layout6, rowID, 2);

    //------------------------------
    rowID ++;
    SetupSpinBox(spbNeutronThreshold[digiID][ch], PSD::CH::NeutronThreshold, -1, true, "Neutron Threshold", layout6, rowID, 0);

  }

  {//@============== input  tab
    inputTab[digiID] = new QTabWidget(digiTab[digiID]);
    chTabWidget[digiID]->addTab(inputTab[digiID], "Input");

    SetupComboBoxTab(cbbOnOff, PHA::CH::ChannelEnable, "On/Off", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbWaveSource, PHA::CH::WaveDataSource, "Wave Data Dource", inputTab[digiID], digiID, digi[digiID]->GetNChannels(), 2);
    SetupComboBoxTab(cbbWaveRes, PHA::CH::WaveResolution,  "Wave Resol.", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbWaveSave, PHA::CH::WaveSaving, "Wave Save", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbDCOffset, PHA::CH::DC_Offset, "DC Offset [%]", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbThreshold, PHA::CH::TriggerThreshold, "Threshold [LSB]", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbRecordLength, PHA::CH::RecordLength, "Record Length [ns]", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbPreTrigger, PHA::CH::PreTrigger, "PreTrigger [ns]", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbParity, PHA::CH::Polarity, "Parity", inputTab[digiID], digiID, digi[digiID]->GetNChannels());

    SetupComboBoxTab(cbbADCInputBaselineAvg, PSD::CH::ADCInputBaselineAvg, "ADC Input BL Avg.", inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbAbsBaseline, PSD::CH::AbsoluteBaseline, "Abs. Baseline",  inputTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbADCInputBaselineGuard, PSD::CH::ADCInputBaselineGuard, "ADC Input BL  Gaurd [ns]", inputTab[digiID], digiID, digi[digiID]->GetNChannels());


    for( int ch = 0; ch < digi[digiID]->GetNChannels(); ch++){
      //Set color of some combox
      cbbOnOff[digiID][ch]->setItemData(1, QBrush(orangeColor), Qt::ForegroundRole);
      connect(cbbOnOff[digiID][ch], &RComboBox::currentIndexChanged, this, [=](int index){ cbbOnOff[ID][ch]->setStyleSheet(index == 1 ? "color : orange;" : "");});
      cbbParity[digiID][ch]->setItemData(1, QBrush(orangeColor), Qt::ForegroundRole);
      connect(cbbParity[digiID][ch], &RComboBox::currentIndexChanged, this, [=](int index){ cbbParity[ID][ch]->setStyleSheet(index == 1 ?  "color : orange;" : "");});
    }

  }
    
  {//@============== PSD Gate  tab
    trapTab[digiID] = new QTabWidget(digiTab[digiID]);
    chTabWidget[digiID]->addTab(trapTab[digiID], "Gate");

    SetupSpinBoxTab(spbGateLong, PSD::CH::GateLongLength, "Long Gate [ns]", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbGateShort, PSD::CH::GateShortLength,  "Short Gate [ns]", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbGateOffset, PSD::CH::GateOffset, "Gate Offset [ns]", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbEnergyGain, PSD::CH::EnergyGain, "Energy Gain", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbLongChargeIntergratorPedestal, PSD::CH::LongChargeIntegratorPedestal, "Long Ped.", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbShortChargeIntergratorPedestal, PSD::CH::ShortChargeIntegratorPedestal, "Short Ped.", trapTab[digiID], digiID, digi[digiID]->GetNChannels());
    
  }

  {//@============== Probe  tab
    probeTab[digiID] = new QTabWidget(digiTab[digiID]);
    chTabWidget[digiID]->addTab(probeTab[digiID], "Probe");

    SetupComboBoxTab(cbbAnaProbe0, PSD::CH::WaveAnalogProbe0,  "Analog Prob. 0",  probeTab[digiID], digiID, digi[digiID]->GetNChannels(), 4);
    SetupComboBoxTab(cbbAnaProbe1, PSD::CH::WaveAnalogProbe1,  "Analog Prob. 1",  probeTab[digiID], digiID, digi[digiID]->GetNChannels(), 4);
    SetupComboBoxTab(cbbDigProbe0, PSD::CH::WaveDigitalProbe0, "Digital Prob. 0", probeTab[digiID], digiID, digi[digiID]->GetNChannels(), 4);
    SetupComboBoxTab(cbbDigProbe1, PSD::CH::WaveDigitalProbe1, "Digital Prob. 1", probeTab[digiID], digiID, digi[digiID]->GetNChannels(), 4);
    SetupComboBoxTab(cbbDigProbe2, PSD::CH::WaveDigitalProbe2, "Digital Prob. 2", probeTab[digiID], digiID, digi[digiID]->GetNChannels(), 4);
    SetupComboBoxTab(cbbDigProbe3, PSD::CH::WaveDigitalProbe3, "Digital Prob. 3", probeTab[digiID], digiID, digi[digiID]->GetNChannels(), 4);
  }

  {//@============== Other  tab
    otherTab[digiID] = new QTabWidget(digiTab[digiID]);
    chTabWidget[digiID]->addTab(otherTab[digiID], "Others");

    SetupComboBoxTab(cbbEventSelector, PHA::CH::EventSelector, "Event Selector", otherTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbWaveSelector, PHA::CH::WaveSelector, "Wave Selector", otherTab[digiID], digiID, digi[digiID]->GetNChannels(), 2 );
    SetupSpinBoxTab(spbEnergySkimLow, PHA::CH::EnergySkimLowDiscriminator, "Energy Skim Low", otherTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbEnergySkimHigh, PHA::CH::EnergySkimHighDiscriminator, "Energy Skim High", otherTab[digiID], digiID, digi[digiID]->GetNChannels());
  }

  {//@============== Trigger  tab
    triggerTab[digiID] = new QTabWidget(digiTab[digiID]);
    chTabWidget[digiID]->addTab(triggerTab[digiID], "Trigger");

    SetupComboBoxTab(cbbEvtTrigger, PHA::CH::EventTriggerSource, "Event Trig. Source", triggerTab[digiID], digiID, digi[digiID]->GetNChannels(), 2);
    SetupComboBoxTab(cbbWaveTrigger, PHA::CH::WaveTriggerSource, "Wave Trig. Source", triggerTab[digiID], digiID, digi[digiID]->GetNChannels(), 2);
    SetupComboBoxTab(cbbChVetoSrc, PHA::CH::ChannelVetoSource, "Veto Source", triggerTab[digiID], digiID, digi[digiID]->GetNChannels(), 2);
    SetupComboBoxTab(cbbCoinMask, PHA::CH::CoincidenceMask, "Coin. Mask", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbAntiCoinMask, PHA::CH::AntiCoincidenceMask, "Anti-Coin. Mask", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbCoinLength, PHA::CH::CoincidenceLength, "Coin. Length [ns]", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbADCVetoWidth, PHA::CH::ADCVetoWidth, "ADC Veto Length [ns]", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());


    SetupComboBoxTab(cbbTriggerFilter, PSD::CH::TriggerFilterSelection, "Trig. Filter", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbTriggerHysteresis, PSD::CH::TriggerHysteresis, "Trig. Hysteresis", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbPileupGap, PSD::CH::PileupGap, "Pile-Up Gap [ns]", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbTimeFilterReTriggerGuard, PSD::CH::TimeFilterRetriggerGuard, "Time Filter Re.Trg Guard [ns]", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbCFDDelay, PSD::CH::CFDDelay, "CFD Delay [ns]", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbCFDFraction, PSD::CH::CFDFraction, "CFD Frac. [%]", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbSmoothingFactor, PSD::CH::SmoothingFactor, "Smoothing Fact.", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbChargeSmooting, PSD::CH::ChargeSmoothing, "Charge Smoothing", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbTimeFilterSmoothing, PSD::CH::TimeFilterSmoothing, "Time Filter Smoothing", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());

    SetupComboBoxTab(cbbEventNeutronReject, PSD::CH::EventNeutronReject, "Event Neutron Rej.", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupComboBoxTab(cbbWaveNeutronReject, PSD::CH::WaveNeutronReject,  "Wave Neutron Rej.", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());
    SetupSpinBoxTab(spbNeutronThreshold, PSD::CH::NeutronThreshold,  "Neutron Threshold", triggerTab[digiID], digiID, digi[digiID]->GetNChannels());

  }

  for( int ch = 0; ch < digi[digiID]->GetNChannels() + 1; ch++) {
    //----- SyncBox
    connect(cbbOnOff[digiID][ch],        &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbOnOff, ch);});
    connect(spbDCOffset[digiID][ch],     &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbDCOffset, ch);});
    connect(spbThreshold[digiID][ch],    &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbThreshold, ch);});
    connect(cbbParity[digiID][ch],       &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbParity, ch);});
    connect(spbRecordLength[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbRecordLength, ch);});
    connect(spbPreTrigger[digiID][ch],   &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbPreTrigger, ch);});
    
    connect(cbbWaveSource[digiID][ch],  &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveSource, ch);});
    connect(cbbWaveRes[digiID][ch],     &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveRes, ch);});
    connect(cbbWaveSave[digiID][ch],    &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveSave, ch);});
    
    connect(cbbAnaProbe0[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbAnaProbe0, ch);});
    connect(cbbAnaProbe1[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbAnaProbe1, ch);});
    connect(cbbDigProbe0[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe0, ch);});
    connect(cbbDigProbe1[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe1, ch);});
    connect(cbbDigProbe2[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe2, ch);});
    connect(cbbDigProbe3[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe3, ch);});

    connect(cbbEventSelector[digiID][ch],  &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbEventSelector, ch);});
    connect(cbbWaveSelector[digiID][ch],   &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveSelector, ch);});
    connect(spbEnergySkimLow[digiID][ch],  &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbEnergySkimLow, ch);});
    connect(spbEnergySkimHigh[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbEnergySkimHigh, ch);});

    connect(cbbEvtTrigger[digiID][ch],   &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbEvtTrigger, ch);});
    connect(cbbWaveTrigger[digiID][ch],  &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveTrigger, ch);});
    connect(cbbChVetoSrc[digiID][ch],    &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbChVetoSrc, ch);});
    connect(cbbCoinMask[digiID][ch],     &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbCoinMask, ch);});
    connect(cbbAntiCoinMask[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbAntiCoinMask, ch);});
    connect(spbCoinLength[digiID][ch],   &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbCoinLength, ch);});
    connect(spbADCVetoWidth[digiID][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbADCVetoWidth, ch);});

    
    connect(cbbADCInputBaselineAvg[digiID][ch],   &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbADCInputBaselineAvg, ch);});
    connect(spbAbsBaseline[digiID][ch],           &RSpinBox::returnPressed,        this, [=](){ SyncSpinBox(spbAbsBaseline, ch);});
    connect(spbADCInputBaselineGuard[digiID][ch], &RSpinBox::returnPressed,        this, [=](){ SyncSpinBox(spbADCInputBaselineGuard, ch);});

    connect(cbbTriggerFilter[digiID][ch],            &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbTriggerFilter, ch);});
    connect(cbbTriggerHysteresis[digiID][ch],        &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbTriggerHysteresis, ch);});
    connect(spbCFDDelay[digiID][ch],                 &RSpinBox::returnPressed,        this, [=](){ SyncSpinBox(spbCFDDelay, ch);});
    connect(spbCFDFraction[digiID][ch],              &RSpinBox::returnPressed,        this, [=](){ SyncSpinBox(spbCFDFraction, ch);});
    connect(cbbSmoothingFactor[digiID][ch],          &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbSmoothingFactor, ch);});
    connect(cbbChargeSmooting[digiID][ch],           &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbChargeSmooting, ch);});
    connect(cbbTimeFilterSmoothing[digiID][ch],      &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbTimeFilterSmoothing, ch);});
    connect(spbTimeFilterReTriggerGuard[digiID][ch], &RSpinBox::returnPressed,        this, [=](){ SyncSpinBox(spbTimeFilterReTriggerGuard, ch);});
    connect(spbPileupGap[digiID][ch],                &RSpinBox::returnPressed,        this, [=](){ SyncSpinBox(spbPileupGap, ch);});

    connect(spbGateLong[digiID][ch],                       &RSpinBox::returnPressed,        this, [=](){ SyncSpinBox(spbGateLong, ch);});
    connect(spbGateShort[digiID][ch],                      &RSpinBox::returnPressed,        this, [=](){ SyncSpinBox(spbGateShort, ch);});
    connect(spbGateOffset[digiID][ch],                     &RSpinBox::returnPressed,        this, [=](){ SyncSpinBox(spbGateOffset, ch);});
    connect(spbLongChargeIntergratorPedestal[digiID][ch],  &RSpinBox::returnPressed,        this, [=](){ SyncSpinBox(spbLongChargeIntergratorPedestal, ch);});
    connect(spbShortChargeIntergratorPedestal[digiID][ch], &RSpinBox::returnPressed,        this, [=](){ SyncSpinBox(spbShortChargeIntergratorPedestal, ch);});
    connect(cbbEnergyGain[digiID][ch],                     &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbEnergyGain, ch);});

    connect(spbNeutronThreshold[digiID][ch],   &RSpinBox::returnPressed,        this, [=](){ SyncSpinBox(spbNeutronThreshold, ch);});
    connect(cbbEventNeutronReject[digiID][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbEventNeutronReject, ch);});
    connect(cbbWaveNeutronReject[digiID][ch],  &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveNeutronReject, ch);});

  }

}

//^================================================================
void DigiSettingsPanel::onTriggerClick(int haha){
  
  unsigned short iDig = haha >> 16;
  unsigned short ch = (haha >> 8 ) & 0xFF;
  unsigned short ch2 = haha & 0xFF;

  //qDebug() << "Digi-" << iDig << ", Ch-" << ch << ", " << ch2; 

  if(trgMapClickStatus[iDig][ch][ch2]){
    trgMap[iDig][ch][ch2]->setStyleSheet(""); 
    trgMapClickStatus[iDig][ch][ch2] = false;
  }else{
    trgMap[iDig][ch][ch2]->setStyleSheet("background-color: red;"); 
    trgMapClickStatus[iDig][ch][ch2] = true;
  }

  //format triggermask for ch;
  unsigned long mask = 0;
  for( int i = 0; i < digi[iDig]->GetNChannels(); i++){
    if( trgMapClickStatus[iDig][ch][i] ) {
      mask += (1ULL << i);
    }
  }

  QString kaka = QString::number(mask);

  QString msg;
  msg = "DIG:" + QString::number(digi[iDig]->GetNChannels()) + ",CH:" + QString::number(ch) + "|" + QString::fromStdString(PHA::CH::ChannelsTriggerMask.GetPara() ) + " = 0x" + QString::number(mask,16);

  if( digi[iDig]->WriteValue(PHA::CH::ChannelsTriggerMask, kaka.toStdString(), ch) ){
    SendLogMsg(msg + "|OK.");
  }else{
    SendLogMsg(msg + "|Fail.");
    digi[iDig]->ReadValue(PHA::CH::ChannelsTriggerMask, ch);
  }

  UpdatePanelFromMemory();
  UpdateOtherPanels();

}

void DigiSettingsPanel::ReadTriggerMap(){

  enableSignalSlot = false;

  //printf("%s\n", __func__);

  // cbAllEvtTrigger[ID]->setCurrentIndex(cbbEvtTrigger[ID][MaxNumberOfChannel]->currentIndex());
  // cbAllWaveTrigger[ID]->setCurrentIndex(cbbWaveTrigger[ID][MaxNumberOfChannel]->currentIndex());
  // cbAllCoinMask[ID]->setCurrentIndex(cbbCoinMask[ID][MaxNumberOfChannel]->currentIndex());
  // cbAllAntiCoinMask[ID]->setCurrentIndex(cbbAntiCoinMask[ID][MaxNumberOfChannel]->currentIndex());
  // sbAllCoinLength[ID]->setValue(spbCoinLength[ID][MaxNumberOfChannel]->value());

  for( int ch = 0; ch < (int) digi[ID]->GetNChannels(); ch ++){

    unsigned long  mask = Utility::TenBase(digi[ID]->GetSettingValueFromMemory(PHA::CH::ChannelsTriggerMask, ch));
    //printf("Trigger Mask of ch-%2d : 0x%s |%s| \n", ch, QString::number(mask, 16).toStdString().c_str(), digi[ID]->GetSettingValueFromMemory(PHA::CH::ChannelsTriggerMask, ch).c_str());

    for( int k = 0; k < (int) digi[ID]->GetNChannels(); k ++ ){
      trgMapClickStatus[ID][ch][k] = ( (mask >> k) & 0x1 );
      if( (mask >> k) & 0x1 ){
        trgMap[ID][ch][k]->setStyleSheet("background-color: red;"); 
      }else{
        trgMap[ID][ch][k]->setStyleSheet("");
      }
    }
  }

  enableSignalSlot = true;
}

//^================================================================

void DigiSettingsPanel::RefreshSettings(){
  printf("DigiSettingsPanel::%s\n", __func__);
  digi[ID]->ReadAllSettings();
  UpdatePanelFromMemory();
}

void DigiSettingsPanel::UpdateStatus(){

  if( tabWidget->currentIndex() >= nDigi) return;

  digi[ID]->ReadValue(PHA::DIG::LED_status);
  digi[ID]->ReadValue(PHA::DIG::ACQ_status);

  for( int i = 0; i < (int) PHA::DIG::TempSensADC.size(); i++){
    if( digi[ID]->GetModelName() != "VX2745" && i > 0 ) continue;
    digi[ID]->ReadValue(PHA::DIG::TempSensADC[i]);
  }
  for( int i = 0; i < (int) PHA::DIG::TempSensOthers.size(); i++){
    digi[ID]->ReadValue(PHA::DIG::TempSensOthers[i]);
  }

  UpdatePanelFromMemory(true);

}

void DigiSettingsPanel::EnableControl(){

  UpdatePanelFromMemory();

  for( int id = 0; id < nDigi; id ++){
    bool enable = !digi[id]->IsAcqOn();

    //digiBox[id]->setEnabled(enable);
    //if( digi[id]->GetFPGAType() == "DPP_PHA") VGABox[id]->setEnabled(enable);
    //if( ckbGlbTrgSource[id][3]->isChecked() ) testPulseBox[id]->setEnabled(enable);

    bdCfg[id]->setEnabled(enable);
    // bdTestPulse[id]->setEnabled(enable);
    bdVGA[id]->setEnabled(enable);
    bdLVDS[id]->setEnabled(enable);
    bdITL[id]->setEnabled(enable);

    box1[id]->setEnabled(enable);
    box3[id]->setEnabled(enable);
    box4[id]->setEnabled(enable);
    box5[id]->setEnabled(enable);
    box6[id]->setEnabled(enable);

    bnReadSettngs[id]->setEnabled(enable);
    bnResetBd[id]->setEnabled(enable);
    bnDefaultSetting[id]->setEnabled(enable);
    bnSaveSettings[id]->setEnabled(enable);
    bnLoadSettings[id]->setEnabled(enable);
    bnClearData[id]->setEnabled(enable);
    bnArmACQ[id]->setEnabled(enable);
    bnDisarmACQ[id]->setEnabled(enable);
    bnSoftwareStart[id]->setEnabled(enable);
    bnSoftwareStop[id]->setEnabled(enable);

    if( digi[id]->GetFPGAType() != "DPP_PHA" || digi[id]->GetModelName() != "VX2745" ) bdVGA[id]->setEnabled(false);

    QVector<QTabWidget*> tempArray = {inputTab[id], trapTab[id], probeTab[id], otherTab[id] };

    for( int k = 0; k < tempArray.size(); k++){
      for( int i = 0; i < tempArray[k]->count(); i++) {
        if( k == 0 && (i == 0 || i == 1 || i == 2 ) ) continue;
        QWidget* currentTab = tempArray[k]->widget(i);
        if( currentTab ){
          QList<QWidget*> childWidgets = currentTab->findChildren<QWidget*>();
          for(int j=0; j<childWidgets.count(); j++) {
              childWidgets[j]->setEnabled(enable);
          }
        }
      }
    }
    
    //triggerMapTab[ID]->setEnabled(enable);

    icBox1->setEnabled(enable);
    icBox2->setEnabled(enable);
  }

}

void DigiSettingsPanel::SaveSettings(){

  //Check path exist
  QDir dir(digiSettingPath);
  if( !dir.exists() ) dir.mkpath(".");

  QString defaultFileName = "setting_" +  QString::number(digi[ID]->GetSerialNumber()) + "_" + QString::fromStdString(digi[ID]->GetFPGAType().substr(4)) + ".dat";

  QString filePath = QFileDialog::getSaveFileName(this, 
                                                  "Save Settings File", 
                                                  QDir::toNativeSeparators(digiSettingPath + "/" + defaultFileName),  
                                                  "Data file (*.dat);;Text files (*.txt);;All files (*.*)");

  if (!filePath.isEmpty()) {

    QFileInfo  fileInfo(filePath);
    QString ext = fileInfo.suffix();
    if( ext == "") filePath += ".dat";

    int flag = digi[ID]->SaveSettingsToFile(filePath.toStdString().c_str());

    switch (flag) {
      case 1 : {
        leSettingFile[ID]->setText(filePath);
        SendLogMsg("Saved setting file <b>" +  filePath + "</b>.");
      }; break;
      case 0 : {
        leSettingFile[ID]->setText("fail to write setting file.");
        SendLogMsg("<font style=\"color:red;\"> Fail to write setting file. <b>" +  filePath + "</b></font>");
      }; break;

      case -1 : {
        leSettingFile[ID]->setText("fail to save setting file, same settings are empty.");
        SendLogMsg("<font style=\"color:red;\"> Fail to save setting file <b>" +  filePath + "</b>, same settings are empty.</font>");
      }; break;
    };

  }

}

void DigiSettingsPanel::LoadSettings(){
  QFileDialog fileDialog(this);
  fileDialog.setDirectory(digiSettingPath);
  fileDialog.setFileMode(QFileDialog::ExistingFile);
  fileDialog.setNameFilter("Data file (*.dat);;Text file (*.txt);;All file (*.*)");
  int result = fileDialog.exec();

  if( ! (result == QDialog::Accepted) ) return;

  if( fileDialog.selectedFiles().size() == 0 ) return; // when no file selected.

  QString fileName = fileDialog.selectedFiles().at(0);

  leSettingFile[ID]->setText(fileName);
  //TODO ==== check is the file valid;

  if( digi[ID]->LoadSettingsFromFile(fileName.toStdString().c_str()) ){
    SendLogMsg("Loaded settings file " + fileName + " for Digi-" + QString::number(digi[ID]->GetSerialNumber()));
    UpdatePanelFromMemory();
    UpdateOtherPanels();
  }else{
    SendLogMsg("Fail to Loaded settings file " + fileName + " for Digi-" + QString::number(digi[ID]->GetSerialNumber()));
  }

}

void DigiSettingsPanel::SetDefaultPHASettigns(){
  SendLogMsg("Program Digitizer-" + QString::number(digi[ID]->GetSerialNumber()) + " to default " + QString::fromStdString(digi[ID]->GetFPGAType()));
  digi[ID]->ProgramBoard();
  digi[ID]->ProgramChannels();
  RefreshSettings();
}

void DigiSettingsPanel::UpdatePanelFromMemory(bool onlyStatus){

  if( !isVisible() ) return;

  enableSignalSlot = false;

  if( onlyStatus){
    //printf("DigiSettingsPanel::%s Digi-%d [Only Board Status]\n", __func__, digi[ID]->GetSerialNumber());
  }else{
    printf("DigiSettingsPanel::%s Digi-%d\n", __func__, digi[ID]->GetSerialNumber());
  }  

  //--------- LED Status
  unsigned int ledStatus = atoi(digi[ID]->GetSettingValueFromMemory(PHA::DIG::LED_status).c_str());
  for( int i = 0; i < 19; i++){
    if( (ledStatus >> i) & 0x1 ) {
      LEDStatus[ID][i]->setStyleSheet("background-color:green;");
    }else{
      LEDStatus[ID][i]->setStyleSheet("");
    }
  }

  //--------- ACQ Status
  unsigned int acqStatus = atoi(digi[ID]->GetSettingValueFromMemory(PHA::DIG::ACQ_status).c_str());
  for( int i = 0; i < 7; i++){
    if( (acqStatus >> i) & 0x1 ) {
      ACQStatus[ID][i]->setStyleSheet("background-color:green;");
    }else{
      ACQStatus[ID][i]->setStyleSheet("");
    }
  }

  //-------- temperature
  for( int i = 0; i < 8; i++){
    leTemp[ID][i]->setText(QString::fromStdString(digi[ID]->GetSettingValueFromMemory(PHA::DIG::TempSensADC[i])));
  }
  
  if( onlyStatus ) {
    enableSignalSlot = true;
    return;
  }

  for (unsigned short j = 0; j < (unsigned short) infoIndex.size(); j++){
    Reg reg = infoIndex[j].second;
    QString text = QString::fromStdString(digi[ID]->ReadValue(reg));
    if( reg.GetPara() == PHA::DIG::ADC_SampleRate.GetPara() ) {
      text += " = " + QString::number(digi[ID]->GetTick2ns(), 'f', 1) + " ns" ;
    }
    leInfo[ID][j]->setText(text);
  } 

  //-------- board settings
  FillComboBoxValueFromMemory(cbbClockSource[ID], PHA::DIG::ClockSource);

  QString result = QString::fromStdString(digi[ID]->GetSettingValueFromMemory(PHA::DIG::StartSource));
  QStringList resultList = result.remove(QChar(' ')).split("|");
  //qDebug() << resultList << "," << resultList.count();
  for( int j = 0; j < (int) PHA::DIG::StartSource.GetAnswers().size(); j++){
    ckbStartSource[ID][j]->setChecked(false);
    for( int i = 0; i < resultList.count(); i++){
      //qDebug() << resultList[i] << ", " << QString::fromStdString((DIGIPARA::DIG::StartSource.GetAnswers())[j].first);
      if( resultList[i] == QString::fromStdString((PHA::DIG::StartSource.GetAnswers())[j].first) ) ckbStartSource[ID][j]->setChecked(true);
    }
  }

  result = QString::fromStdString(digi[ID]->GetSettingValueFromMemory(PHA::DIG::GlobalTriggerSource));
  resultList = result.remove(QChar(' ')).split("|");
  bdTestPulse[ID]->setEnabled(false);
  for( int j = 0; j < (int) PHA::DIG::GlobalTriggerSource.GetAnswers().size(); j++){
    ckbGlbTrgSource[ID][j]->setChecked(false);
    for( int i = 0; i < resultList.count(); i++){
      if( resultList[i] == QString::fromStdString((PHA::DIG::GlobalTriggerSource.GetAnswers())[j].first) ) {
        ckbGlbTrgSource[ID][j]->setChecked(true);
        if( resultList[i] == "TestPulse" ||  cbDACoutMode[ID]->currentData().toString().toStdString() == "Square" ) bdTestPulse[ID]->setEnabled(true);
      }
    }
  }

  FillComboBoxValueFromMemory(cbbTrgOut[ID], PHA::DIG::TrgOutMode);
  FillComboBoxValueFromMemory(cbbGPIO[ID], PHA::DIG::GPIOMode);
  FillComboBoxValueFromMemory(cbbBusyIn[ID], PHA::DIG::BusyInSource);
  FillComboBoxValueFromMemory(cbbSyncOut[ID], PHA::DIG::SyncOutMode);
  FillComboBoxValueFromMemory(cbbAutoDisarmAcq[ID], PHA::DIG::EnableAutoDisarmACQ);
  FillComboBoxValueFromMemory(cbbStatEvents[ID], PHA::DIG::EnableStatisticEvents);
  FillComboBoxValueFromMemory(cbbBdVetoPolarity[ID], PHA::DIG::BoardVetoPolarity);
  FillComboBoxValueFromMemory(cbbBoardVetoSource[ID], PHA::DIG::BoardVetoSource);
  FillComboBoxValueFromMemory(cbbIOLevel[ID], PHA::DIG::IO_Level);

  FillSpinBoxValueFromMemory(dsbBdVetoWidth[ID], PHA::DIG::BoardVetoWidth);
  FillSpinBoxValueFromMemory(spbRunDelay[ID], PHA::DIG::RunDelay);
  FillSpinBoxValueFromMemory(dsbVolatileClockOutDelay[ID], PHA::DIG::VolatileClockOutDelay);
  FillSpinBoxValueFromMemory(dsbClockOutDelay[ID], PHA::DIG::PermanentClockOutDelay);

  //------------- test pulse
  FillSpinBoxValueFromMemory(dsbTestPuslePeriod[ID], PHA::DIG::TestPulsePeriod);
  FillSpinBoxValueFromMemory(dsbTestPusleWidth[ID], PHA::DIG::TestPulseWidth);
  FillSpinBoxValueFromMemory(spbTestPusleLowLevel[ID], PHA::DIG::TestPulseLowLevel);
  FillSpinBoxValueFromMemory(spbTestPusleHighLevel[ID], PHA::DIG::TestPulseHighLevel);

  //------------- ITL
  FillComboBoxValueFromMemory(cbITLAMainLogic[ID], PHA::DIG::ITLAMainLogic);
  FillComboBoxValueFromMemory(cbITLAPairLogic[ID], PHA::DIG::ITLAPairLogic);
  FillComboBoxValueFromMemory(cbITLAPolarity[ID],  PHA::DIG::ITLAPolarity);
  FillSpinBoxValueFromMemory( sbITLAMajority[ID],  PHA::DIG::ITLAMajorityLev);
  FillSpinBoxValueFromMemory( sbITLAGateWidth[ID], PHA::DIG::ITLAGateWidth);

  FillComboBoxValueFromMemory(cbITLBMainLogic[ID], PHA::DIG::ITLBMainLogic);
  FillComboBoxValueFromMemory(cbITLBPairLogic[ID], PHA::DIG::ITLBPairLogic);
  FillComboBoxValueFromMemory(cbITLBPolarity[ID],  PHA::DIG::ITLBPolarity);
  FillSpinBoxValueFromMemory( sbITLBMajority[ID],  PHA::DIG::ITLBMajorityLev);
  FillSpinBoxValueFromMemory( sbITLBGateWidth[ID], PHA::DIG::ITLBGateWidth);

  //------------- LVDS
  for( int k = 0; k < 4; k ++){
    FillComboBoxValueFromMemory(cbLVDSMode[ID][k], PHA::LVDS::LVDSMode, k);
    FillComboBoxValueFromMemory(cbLVDSDirection[ID][k], PHA::LVDS::LVDSDirection, k);
  }
  leLVDSIOReg[ID]->setText(QString::fromStdString(digi[ID]->GetSettingValueFromMemory(PHA::DIG::LVDSIOReg)));

  //------------- DAC 
  FillComboBoxValueFromMemory(cbDACoutMode[ID], PHA::DIG::DACoutMode);
  FillSpinBoxValueFromMemory(sbDACoutStaticLevel[ID], PHA::DIG::DACoutStaticLevel);
  FillSpinBoxValueFromMemory(sbDACoutChSelect[ID], PHA::DIG::DACoutChSelect);

  if( cbDACoutMode[ID]->currentData().toString().toStdString() == "Static" ) {
    sbDACoutStaticLevel[ID]->setEnabled(true);
  }else{
    sbDACoutStaticLevel[ID]->setEnabled(false);
  }

  if( cbDACoutMode[ID]->currentData().toString().toStdString() == "ChInput"  ) {
    sbDACoutChSelect[ID]->setEnabled(true);
  }else{
    sbDACoutChSelect[ID]->setEnabled(false);
  }

  //------------ Group
  if( digi[ID]->GetCupVer() >= MIN_VERSION_GROUP ){
    for( int k = 0 ; k < MaxNumberOfGroup; k++){
      FillSpinBoxValueFromMemory(spbInputDelay[ID][k], PHA::GROUP::InputDelay, k); // PHA = PSD
    }
  }


  //@============================== Channel setting/ status

  for( int ch = 0; ch < digi[ID]->GetNChannels(); ch++){

    unsigned int status = atoi(digi[ID]->GetSettingValueFromMemory(PHA::CH::ChannelStatus).c_str());
    for( int i = 0; i < 9; i++){
      if( (status >> i) & 0x1 ) {
        chStatus[ID][ch][i]->setStyleSheet("background-color:green;");
      }else{
        chStatus[ID][ch][i]->setStyleSheet("");
      }
    }
    chGainFactor[ID][ch]->setText(QString::fromStdString(digi[ID]->GetSettingValueFromMemory(PHA::CH::GainFactor, ch)));
    chADCToVolts[ID][ch]->setText(QString::fromStdString(digi[ID]->GetSettingValueFromMemory(PHA::CH::ADCToVolts, ch)));

    FillComboBoxValueFromMemory(cbbOnOff[ID][ch], PHA::CH::ChannelEnable, ch);
    FillSpinBoxValueFromMemory(spbRecordLength[ID][ch], PHA::CH::RecordLength, ch);
    FillSpinBoxValueFromMemory(spbPreTrigger[ID][ch], PHA::CH::PreTrigger, ch);
    FillSpinBoxValueFromMemory(spbDCOffset[ID][ch], PHA::CH::DC_Offset, ch);
    FillSpinBoxValueFromMemory(spbThreshold[ID][ch], PHA::CH::TriggerThreshold, ch);

    FillComboBoxValueFromMemory(cbbParity[ID][ch], PHA::CH::Polarity, ch);
    FillComboBoxValueFromMemory(cbbWaveSource[ID][ch], PHA::CH::WaveDataSource, ch);
    FillComboBoxValueFromMemory(cbbWaveRes[ID][ch], PHA::CH::WaveResolution, ch);
    FillComboBoxValueFromMemory(cbbWaveSave[ID][ch], PHA::CH::WaveSaving, ch);

    FillComboBoxValueFromMemory(cbbEvtTrigger[ID][ch], PHA::CH::EventTriggerSource, ch);
    FillComboBoxValueFromMemory(cbbWaveTrigger[ID][ch], PHA::CH::WaveTriggerSource, ch);
    FillComboBoxValueFromMemory(cbbCoinMask[ID][ch], PHA::CH::CoincidenceMask, ch);
    FillComboBoxValueFromMemory(cbbAntiCoinMask[ID][ch], PHA::CH::AntiCoincidenceMask, ch);
    FillSpinBoxValueFromMemory(spbCoinLength[ID][ch], PHA::CH::CoincidenceLength, ch);

    FillComboBoxValueFromMemory(cbbChVetoSrc[ID][ch], PHA::CH::ChannelVetoSource, ch);
    FillSpinBoxValueFromMemory(spbADCVetoWidth[ID][ch], PHA::CH::ADCVetoWidth, ch);

    FillComboBoxValueFromMemory(cbbEventSelector[ID][ch], PHA::CH::EventSelector, ch);
    FillComboBoxValueFromMemory(cbbWaveSelector[ID][ch], PHA::CH::WaveSelector, ch);
    FillSpinBoxValueFromMemory(spbEnergySkimLow[ID][ch], PHA::CH::EnergySkimLowDiscriminator, ch);
    FillSpinBoxValueFromMemory(spbEnergySkimHigh[ID][ch], PHA::CH::EnergySkimHighDiscriminator, ch);

    FillComboBoxValueFromMemory(cbbAnaProbe0[ID][ch], PHA::CH::WaveAnalogProbe0, ch);
    FillComboBoxValueFromMemory(cbbAnaProbe1[ID][ch], PHA::CH::WaveAnalogProbe1, ch);
    FillComboBoxValueFromMemory(cbbDigProbe0[ID][ch], PHA::CH::WaveDigitalProbe0, ch);
    FillComboBoxValueFromMemory(cbbDigProbe1[ID][ch], PHA::CH::WaveDigitalProbe1, ch);
    FillComboBoxValueFromMemory(cbbDigProbe2[ID][ch], PHA::CH::WaveDigitalProbe2, ch);
    FillComboBoxValueFromMemory(cbbDigProbe3[ID][ch], PHA::CH::WaveDigitalProbe3, ch);

    std::string itlConnect = digi[ID]->GetSettingValueFromMemory(PHA::CH::ITLConnect, ch);
    if( itlConnect == "Disabled" ) {
      ITLConnectStatus[ID][ch] = 0;
      chITLConnect[ID][ch][0]->setStyleSheet("");
      chITLConnect[ID][ch][1]->setStyleSheet("");
    }
    if( itlConnect == "ITLA" ) {
      ITLConnectStatus[ID][ch] = 1;
      chITLConnect[ID][ch][0]->setStyleSheet("background-color : green;");
      chITLConnect[ID][ch][1]->setStyleSheet("");
    }
    if( itlConnect == "ITLB" ) {
      ITLConnectStatus[ID][ch] = 2;
      chITLConnect[ID][ch][0]->setStyleSheet("");
      chITLConnect[ID][ch][1]->setStyleSheet("background-color : green;");
    }

    if( digi[ID]->GetFPGAType() == DPPType::PHA ) {

      FillSpinBoxValueFromMemory(spbInputRiseTime[ID][ch], PHA::CH::TimeFilterRiseTime, ch);
      FillSpinBoxValueFromMemory(spbTriggerGuard[ID][ch], PHA::CH::TimeFilterRetriggerGuard, ch);
      FillComboBoxValueFromMemory(cbbLowFilter[ID][ch], PHA::CH::EnergyFilterLowFreqFilter, ch);

      FillSpinBoxValueFromMemory(spbTrapRiseTime[ID][ch], PHA::CH::EnergyFilterRiseTime, ch);
      FillSpinBoxValueFromMemory(spbTrapFlatTop[ID][ch], PHA::CH::EnergyFilterFlatTop, ch);
      FillSpinBoxValueFromMemory(spbTrapPoleZero[ID][ch], PHA::CH::EnergyFilterPoleZero, ch);
      
      FillSpinBoxValueFromMemory(spbPeaking[ID][ch], PHA::CH::EnergyFilterPeakingPosition, ch);
      FillSpinBoxValueFromMemory(spbBaselineGuard[ID][ch], PHA::CH::EnergyFilterBaselineGuard, ch);
      FillSpinBoxValueFromMemory(spbPileupGuard[ID][ch], PHA::CH::EnergyFilterPileUpGuard, ch);
      
      FillComboBoxValueFromMemory(cbbBaselineAvg[ID][ch], PHA::CH::EnergyFilterBaselineAvg, ch);
      FillSpinBoxValueFromMemory(spbFineGain[ID][ch], PHA::CH::EnergyFilterFineGain, ch);
      FillComboBoxValueFromMemory(cbbPeakingAvg[ID][ch], PHA::CH::EnergyFilterPeakingAvg, ch);

    }

    if( digi[ID]->GetFPGAType() == DPPType::PSD){

      FillComboBoxValueFromMemory(cbbADCInputBaselineAvg[ID][ch], PSD::CH::ADCInputBaselineAvg, ch);
      FillSpinBoxValueFromMemory(spbAbsBaseline[ID][ch], PSD::CH::AbsoluteBaseline, ch);
      FillSpinBoxValueFromMemory(spbADCInputBaselineGuard[ID][ch], PSD::CH::ADCInputBaselineGuard, ch);

      FillComboBoxValueFromMemory(cbbTriggerFilter[ID][ch], PSD::CH::TriggerFilterSelection, ch);
      FillComboBoxValueFromMemory(cbbTriggerHysteresis[ID][ch], PSD::CH::TriggerHysteresis, ch);
      FillSpinBoxValueFromMemory(spbCFDDelay[ID][ch], PSD::CH::CFDDelay, ch);
      FillSpinBoxValueFromMemory(spbCFDFraction[ID][ch], PSD::CH::CFDFraction, ch);
      FillComboBoxValueFromMemory(cbbSmoothingFactor[ID][ch], PSD::CH::SmoothingFactor, ch);
      FillComboBoxValueFromMemory(cbbChargeSmooting[ID][ch], PSD::CH::ChargeSmoothing, ch);
      FillComboBoxValueFromMemory(cbbTimeFilterSmoothing[ID][ch], PSD::CH::TimeFilterSmoothing, ch);
      FillSpinBoxValueFromMemory(spbTimeFilterReTriggerGuard[ID][ch], PSD::CH::TimeFilterRetriggerGuard, ch);
      FillSpinBoxValueFromMemory(spbPileupGap[ID][ch], PSD::CH::PileupGap, ch);

      FillSpinBoxValueFromMemory(spbGateLong[ID][ch], PSD::CH::GateLongLength, ch);
      FillSpinBoxValueFromMemory(spbGateShort[ID][ch], PSD::CH::GateShortLength, ch);
      FillSpinBoxValueFromMemory(spbGateOffset[ID][ch], PSD::CH::GateOffset, ch);
      FillSpinBoxValueFromMemory(spbLongChargeIntergratorPedestal[ID][ch], PSD::CH::LongChargeIntegratorPedestal, ch);
      FillSpinBoxValueFromMemory(spbShortChargeIntergratorPedestal[ID][ch], PSD::CH::ShortChargeIntegratorPedestal, ch);
      FillComboBoxValueFromMemory(cbbEnergyGain[ID][ch], PSD::CH::EnergyGain, ch);

      FillSpinBoxValueFromMemory(spbNeutronThreshold[ID][ch], PSD::CH::NeutronThreshold, ch);
      FillComboBoxValueFromMemory(cbbEventNeutronReject[ID][ch], PSD::CH::EventNeutronReject, ch);
      FillComboBoxValueFromMemory(cbbWaveNeutronReject[ID][ch], PSD::CH::WaveNeutronReject, ch);
    }
  }

  //------ Trigger Mask
  if( cbChPick[ID]->currentData().toInt() < 0 ) {
    unsigned long mask = Utility::TenBase(digi[ID]->GetSettingValueFromMemory(PHA::CH::ChannelsTriggerMask, 0));
    
    bool isSame = true;
    for(int ch = 1; ch < digi[ID]->GetNChannels() ; ch ++){
      unsigned long haha = Utility::TenBase(digi[ID]->GetSettingValueFromMemory(PHA::CH::ChannelsTriggerMask, ch));
      if( mask != haha) {
        isSame = false;
        leTriggerMask[ID][MaxNumberOfChannel]->setText("Diff. value");
        break;
      }
    }

    if( isSame ) leTriggerMask[ID][MaxNumberOfChannel]->setText("0x" + QString::number(mask, 16).toUpper());
  }else{
    unsigned long  mask = Utility::TenBase(digi[ID]->GetSettingValueFromMemory(PHA::CH::ChannelsTriggerMask, cbChPick[ID]->currentData().toInt()));
    leTriggerMask[ID][digi[ID]->GetNChannels()]->setText("0x" + QString::number(mask, 16).toUpper());
    leTriggerMask[ID][digi[ID]->GetNChannels()]->setStyleSheet("");
  }

  enableSignalSlot = true;

  ReadTriggerMap();

  if( cbChPick[ID]->currentData().toInt() >= 0 ) return;

  SyncComboBox(cbbOnOff       , -1);
  SyncSpinBox(spbDCOffset     , -1);
  SyncSpinBox(spbThreshold    , -1);
  SyncComboBox(cbbParity      , -1);
  SyncSpinBox(spbRecordLength , -1);
  SyncSpinBox(spbPreTrigger   , -1);

  SyncComboBox(cbbWaveSource, -1);
  SyncComboBox(cbbWaveRes,    -1);
  SyncComboBox(cbbWaveSave,   -1);

  SyncComboBox(cbbAnaProbe0, -1);
  SyncComboBox(cbbAnaProbe1, -1);
  SyncComboBox(cbbDigProbe0, -1);
  SyncComboBox(cbbDigProbe1, -1);
  SyncComboBox(cbbDigProbe2, -1);
  SyncComboBox(cbbDigProbe3, -1);

  SyncComboBox(cbbEventSelector, -1);
  SyncComboBox(cbbWaveSelector , -1);
  SyncSpinBox(spbEnergySkimHigh, -1);
  SyncSpinBox(spbEnergySkimLow , -1);

  SyncComboBox(cbbEvtTrigger   , -1);
  SyncComboBox(cbbWaveTrigger  , -1);
  SyncComboBox(cbbChVetoSrc    , -1);
  SyncComboBox(cbbCoinMask     , -1);
  SyncComboBox(cbbAntiCoinMask , -1);
  SyncSpinBox(spbCoinLength    , -1);
  SyncSpinBox(spbADCVetoWidth  , -1);

  if( digi[ID]->GetFPGAType() == DPPType::PHA){  
    SyncSpinBox(spbInputRiseTime , -1);
    SyncSpinBox(spbTriggerGuard  , -1);
    SyncComboBox(cbbLowFilter    , -1);

    SyncSpinBox(spbTrapRiseTime  , -1);
    SyncSpinBox(spbTrapFlatTop   , -1);
    SyncSpinBox(spbTrapPoleZero  , -1);
    
    SyncSpinBox(spbPeaking       , -1);
    SyncSpinBox(spbBaselineGuard , -1);
    SyncSpinBox(spbPileupGuard   , -1);
    
    SyncComboBox(cbbBaselineAvg  , -1);
    SyncComboBox(cbbPeakingAvg   , -1);
    SyncSpinBox(spbFineGain      , -1);
  }

  if( digi[ID]->GetFPGAType() == DPPType::PSD){  


    SyncComboBox(cbbADCInputBaselineAvg , -1);
    SyncSpinBox(spbAbsBaseline          , -1);
    SyncSpinBox(spbADCInputBaselineGuard, -1);

    SyncComboBox(cbbTriggerFilter          , -1);
    SyncComboBox(cbbTriggerHysteresis      , -1);
    SyncSpinBox(spbCFDDelay                , -1);
    SyncSpinBox(spbCFDFraction             , -1);
    SyncComboBox(cbbSmoothingFactor        , -1);
    SyncComboBox(cbbChargeSmooting         , -1);
    SyncComboBox(cbbTimeFilterSmoothing    , -1);
    SyncSpinBox(spbTimeFilterReTriggerGuard, -1);
    SyncSpinBox(spbPileupGap               , -1);

    SyncSpinBox(spbGateLong                      , -1);
    SyncSpinBox(spbGateShort                     , -1);
    SyncSpinBox(spbGateOffset                    , -1);
    SyncSpinBox(spbLongChargeIntergratorPedestal , -1);
    SyncSpinBox(spbShortChargeIntergratorPedestal, -1);
    SyncComboBox(cbbEnergyGain                   , -1);

    SyncSpinBox(spbNeutronThreshold   , -1);
    SyncComboBox(cbbEventNeutronReject, -1);
    SyncComboBox(cbbWaveNeutronReject , -1);


  }

}
//^###########################################################################

void DigiSettingsPanel::SetStartSource(){
  if( !enableSignalSlot ) return;

  std::string value = "";
  for( int i = 0; i < (int) PHA::DIG::StartSource.GetAnswers().size(); i++){
    if( ckbStartSource[ID][i]->isChecked() ){
      //printf("----- %s \n", DIGIPARA::DIG::StartSource.GetAnswers()[i].first.c_str());
      if( value != "" ) value += " | ";
      value += PHA::DIG::StartSource.GetAnswers()[i].first;
    }
  }

  //printf("================ %s\n", value.c_str());
  QString msg;
  msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" +  QString::fromStdString(PHA::DIG::StartSource.GetPara());
  msg += " = " + QString::fromStdString(value);

  if( digi[ID]->WriteValue(PHA::DIG::StartSource, value) ){
    SendLogMsg(msg + "|OK.");
    for( int i = 0; i < (int) PHA::DIG::StartSource.GetAnswers().size(); i++){
      ckbStartSource[ID][i]->setStyleSheet("");
    }
  }else{
    SendLogMsg(msg + "|Fail.");
    for( int i = 0; i < (int) PHA::DIG::StartSource.GetAnswers().size(); i++){
      ckbStartSource[ID][i]->setStyleSheet("background-color : red");
    }
  }
}

void DigiSettingsPanel::SetGlobalTriggerSource(){
  if( !enableSignalSlot ) return;

  std::string value = "";
  if( cbDACoutMode[ID]->currentData().toString().toStdString() != "Square") bdTestPulse[ID]->setEnabled(false);
  for( int i = 0; i < (int) PHA::DIG::GlobalTriggerSource.GetAnswers().size(); i++){
    if( ckbGlbTrgSource[ID][i]->isChecked() ){
      //printf("----- %s \n", DIGIPARA::DIG::StartSource.GetAnswers()[i].first.c_str());
      if( value != "" ) value += " | ";
      value += PHA::DIG::GlobalTriggerSource.GetAnswers()[i].first;
      if( PHA::DIG::GlobalTriggerSource.GetAnswers()[i].first == "TestPulse" ) bdTestPulse[ID]->setEnabled(true);
    }
  }

  //printf("================ %s\n", value.c_str());
  QString msg;
  msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(PHA::DIG::GlobalTriggerSource.GetPara());
  msg += " = " + QString::fromStdString(value);
  SendLogMsg(msg);

  if( digi[ID]->WriteValue(PHA::DIG::GlobalTriggerSource, value) ){
    SendLogMsg(msg + "|OK.");
    for( int i = 0; i < (int) PHA::DIG::GlobalTriggerSource.GetAnswers().size(); i++){
      ckbGlbTrgSource[ID][i]->setStyleSheet("");
    }
  }else{
    SendLogMsg(msg + "|Fail.");
    for( int i = 0; i < (int) PHA::DIG::GlobalTriggerSource.GetAnswers().size(); i++){
      ckbGlbTrgSource[ID][i]->setStyleSheet("background-color : red");
    }
  }

}


//^###########################################################################
void DigiSettingsPanel::SetupShortComboBox(RComboBox *&cbb, Reg para){
  for( int i = 0 ; i < (int) para.GetAnswers().size(); i++){
    cbb->addItem(QString::fromStdString((para.GetAnswers())[i].second), 
                QString::fromStdString((para.GetAnswers())[i].first));
  }
}

void DigiSettingsPanel::SetupComboBox(RComboBox *&cbb, const Reg para, int ch_index, bool isMaster, QString labelTxt, QGridLayout *layout, int row, int col, int srow, int scol){
  QLabel * lb = new QLabel(labelTxt, this); 
  layout->addWidget(lb, row, col);
  lb->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  cbb = new RComboBox(this);
  layout->addWidget(cbb, row, col+1, srow, scol);
  for( int i = 0 ; i < (int) para.GetAnswers().size(); i++){
    cbb->addItem(QString::fromStdString((para.GetAnswers())[i].second), QString::fromStdString((para.GetAnswers())[i].first));
  }
  if( ch_index == -1 && para.GetType() == TYPE::CH ) cbb->addItem("");
  connect(cbb, &RComboBox::currentIndexChanged, this, [=](){
    if( !enableSignalSlot ) return;
    int index = ( ch_index == -1 && isMaster ?  cbChPick[ID]->currentData().toInt() : ch_index);
    //int index = ch_index;
    //printf("%s %d  %s \n", para.GetPara().c_str(), index, cbb->currentData().toString().toStdString().c_str());
    QString msg;
    msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(para.GetPara());
    if( para.GetType() == TYPE::CH ) msg += ",CH:" + (index == -1 ? "All" : QString::number(index));
    if( para.GetType() == TYPE::VGA ) msg += ",VGA:" + QString::number(index);
    msg += " = " + cbb->currentData().toString();
    if( digi[ID]->WriteValue(para, cbb->currentData().toString().toStdString(), index)){
      SendLogMsg(msg + "|OK.");
      cbb->setStyleSheet("");
      UpdatePanelFromMemory();
      UpdateOtherPanels();
    }else{
      SendLogMsg(msg + "|Fail.");
      cbb->setStyleSheet("color:red;");
    }
  });
}

void DigiSettingsPanel::SetupSpinBox(RSpinBox *&spb, const Reg para, int ch_index, bool isMaster, QString labelTxt, QGridLayout *layout, int row, int col, int srow, int scol){
  QLabel * lb = new QLabel(labelTxt, this); 
  layout->addWidget(lb, row, col);
  lb->setAlignment(Qt::AlignRight| Qt::AlignCenter);
  spb = new RSpinBox(this);  
  if( isMaster ){
    spb->setMinimum(qMin(-1.0, atof( para.GetAnswers()[0].first.c_str())));
  }else{
    spb->setMinimum(atof( para.GetAnswers()[0].first.c_str()));
  }
  spb->setMaximum(atof( para.GetAnswers()[1].first.c_str()));

  if( para.GetAnswers().size()  >= 3 ) {
    spb->setSingleStep(atof(para.GetAnswers()[2].first.c_str()));
  }else{
    printf("--- missed. %s\n", para.GetPara().c_str());
  }
  spb->SetToolTip( atof( para.GetAnswers()[0].first.c_str()));
  layout->addWidget(spb, row, col + 1, srow, scol);

  connect(spb, &RSpinBox::valueChanged, this, [=](){ 
    if( !enableSignalSlot ) return; 
    spb->setStyleSheet("color:blue;");
  });
  connect(spb, &RSpinBox::returnPressed, this, [=](){
    if( !enableSignalSlot ) return;
    //printf("%s %d  %d \n", para.GetPara().c_str(), index, spb->value());
    if( spb->decimals() == 0 && spb->singleStep() != 1) {
      double step = spb->singleStep();
      double value = spb->value();
      spb->setValue( (std::round(value/step) * step) );
    }
    int index = ( ch_index == -1 && isMaster ?  cbChPick[ID]->currentData().toInt() : ch_index);
    QString msg;
    msg = "DIG:"+ QString::number(digi[ID]->GetSerialNumber()) + "|" + QString::fromStdString(para.GetPara()) ;
    if( para.GetType() == TYPE::CH ) msg += ",CH:" + (index == -1 ? "All" : QString::number(index));
    msg += " = " + QString::number(spb->value());

    if( para.GetPara() == PHA::GROUP::InputDelay.GetPara() ){

      if( digi[ID]->WriteValue(para, std::to_string(spb->value()/8), index)){
        SendLogMsg(msg + "|OK.");
        spb->setStyleSheet("");
        UpdatePanelFromMemory();
        UpdateOtherPanels();
      }else{
        SendLogMsg(msg + "|Fail.");
        spb->setStyleSheet("color:red;");
      }

      // printf("============%d| %s \n", index, digi[ID]->GetSettingValueFromMemory(PHA::GROUP::InputDelay, index).c_str());


    }else{
      if( digi[ID]->WriteValue(para, std::to_string(spb->value()), index)){
        SendLogMsg(msg + "|OK.");
        spb->setStyleSheet("");
        UpdatePanelFromMemory();
        UpdateOtherPanels();
      }else{
        SendLogMsg(msg + "|Fail.");
        spb->setStyleSheet("color:red;");
      }
    }
  });
}

void DigiSettingsPanel::SyncComboBox(RComboBox *(&cbb)[][MaxNumberOfChannel + 1], int ch){
  if( !enableSignalSlot ) return;
  if( cbChPick[ID]->currentData().toInt() >= 0 ) return;

  const int nCh = digi[ID]->GetNChannels();

  if( ch == nCh ){
    const int index = cbb[ID][nCh]->currentIndex();
    if( cbb[ID][nCh]->currentText() == "" ) return;  
    enableSignalSlot = false;
    for( int i = 0; i < nCh; i++) cbb[ID][i]->setCurrentIndex(index);
    enableSignalSlot = true;
  }else{
    //check is all ComboBox has same index;
    int count = 1;
    const int index = cbb[ID][0]->currentIndex();
    for( int i = 1; i < nCh; i ++){
      if( cbb[ID][i]->currentIndex() == index ) count++;
    }

    enableSignalSlot = false;
    if( count != nCh ){
       cbb[ID][nCh]->setCurrentText("");
    }else{
       cbb[ID][nCh]->setCurrentIndex(index);
    }
    enableSignalSlot = true;
  }
}

void DigiSettingsPanel::SyncSpinBox(RSpinBox *(&spb)[][MaxNumberOfChannel+1], int ch){
  if( !enableSignalSlot ) return;

  const int nCh = digi[ID]->GetNChannels();

  if( ch == nCh ){
    const int value = spb[ID][nCh]->value();
    if( spb[ID][nCh]->value() == -999 ) return;  
    enableSignalSlot = false;
    for( int i = 0; i < nCh; i++) spb[ID][i]->setValue(value);
    enableSignalSlot = true;
  }else{
    //check is all ComboBox has same index;
    int count = 1;
    const int value = spb[ID][0]->value();
    for( int i = 1; i < nCh; i ++){
      if( spb[ID][i]->value() == value ) count++;
    }

    //printf("%d =? %d \n", count, nCh);
    enableSignalSlot = false;
    if( count != nCh ){
       spb[ID][nCh]->setValue(-1);
    }else{
       spb[ID][nCh]->setValue(value);
    }
    enableSignalSlot = true;
  }
}

void DigiSettingsPanel::SetupSpinBoxTab(RSpinBox *(&spb)[][MaxNumberOfChannel+1], const Reg para, QString text, QTabWidget *tabWidget, int iDigi, int nChannel){
  QWidget * tabPage = new QWidget(this); tabWidget->addTab(tabPage, text);
  QGridLayout * allLayout = new QGridLayout(tabPage); 
  //allLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  allLayout->setAlignment(Qt::AlignTop);
  allLayout->setHorizontalSpacing(10);
  allLayout->setVerticalSpacing(0);
  for( int ch = 0; ch < nChannel; ch++){
    SetupSpinBox(spb[iDigi][ch], para, ch, false, "ch-"+QString::number(ch)+ "  ", allLayout, ch/4, ch%4 * 2);
  }
}

void DigiSettingsPanel::SetupComboBoxTab(RComboBox *(&cbb)[][MaxNumberOfChannel + 1], const Reg para, QString text, QTabWidget *tabWidget, int iDigi, int nChannel, int nCol){
  QWidget * tabPage = new QWidget(this); tabWidget->addTab(tabPage, text);
  QGridLayout * allLayout = new QGridLayout(tabPage); 
  //allLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  allLayout->setAlignment(Qt::AlignTop);
  allLayout->setHorizontalSpacing(10);
  allLayout->setVerticalSpacing(0);
  for( int ch = 0; ch < nChannel; ch++){
    SetupComboBox(cbb[iDigi][ch], para, ch, false, "ch-"+QString::number(ch) + "  ", allLayout, ch/nCol, ch%nCol * 3);
  }
}

void DigiSettingsPanel::FillComboBoxValueFromMemory(RComboBox *&cbb, const Reg para, int ch_index){
  QString result = QString::fromStdString(digi[ID]->GetSettingValueFromMemory(para, ch_index));
  //printf("%s === %s, %d, %p\n", __func__, result.toStdString().c_str(), ID, cbb);
  int index = cbb->findData(result);
  if( index >= 0 && index < cbb->count()) {
    cbb->setCurrentIndex(index);
  }else{
    //printf("%s  %s\n", para.GetPara().c_str(), result.toStdString().c_str());
  }
}

void DigiSettingsPanel::FillSpinBoxValueFromMemory(RSpinBox *&spb, const Reg para, int ch_index){
  QString result = QString::fromStdString(digi[ID]->GetSettingValueFromMemory(para, ch_index));
  //printf("%s === %s, %d, %p\n", __func__, result.toStdString().c_str(), ID, spb);

  if( para.GetPara() == PHA::GROUP::InputDelay.GetPara() && digi[ID]->GetCupVer() >= MIN_VERSION_GROUP) {
    spb->setValue(result.toDouble()*8);
  }else{
    spb->setValue(result.toDouble());
  }
}

void DigiSettingsPanel::ReadBoardSetting(int cbIndex){

  if( enableSignalSlot == false ) return;

  enableSignalSlot = false;

  // PHA and PSD has same board setting, but for furture extension
  int ID = cbIQDigi->currentIndex();
  std::vector<Reg> bdSettings ;

  if( digi[ID]->GetFPGAType() == DPPType::PHA ) {
    bdSettings = PHA::DIG::AllSettings;
  }else if( digi[ID]->GetFPGAType() == DPPType::PSD ) {
    bdSettings = PSD::DIG::AllSettings;
  }else{
    enableSignalSlot = true;
    return;
  }

  QString type;
  switch (bdSettings[cbIndex].ReadWrite()) {
    case RW::ReadOnly : type ="Read Only"; break;
    case RW::WriteOnly : type ="Write Only"; break;
    case RW::ReadWrite : type ="Read/Write"; break;
  }
  leBdSettingsType->setText(type);

  QString ans = QString::fromStdString(digi[ID]->ReadValue(bdSettings[cbIndex]));
  ANSTYPE haha = bdSettings[cbIndex].GetAnswerType();

  if( haha == ANSTYPE::BYTE){
    leBdSettingsRead->setText( "0x" + QString::number(ans.toULong(), 16).rightJustified(16, '0'));
  }else if( haha == ANSTYPE::BINARY ){
    leBdSettingsRead->setText( "0b" + QString::number(ans.toUInt(), 2).rightJustified(18, '0'));
  }else{
    leBdSettingsRead->setText(ans);
  }
  leBdSettingsUnit->setText(QString::fromStdString(bdSettings[cbIndex].GetUnit()));

  if( bdSettings[cbIndex].ReadWrite() != RW::ReadOnly && haha != ANSTYPE::NONE ){

    //===== spin box
    if( haha == ANSTYPE::FLOAT || haha == ANSTYPE::INTEGER ){
      cbBdAns->clear();
      cbBdAns->setEnabled(false);
      leBdSettingsWrite->setEnabled(false);
      leBdSettingsWrite->clear();
      sbBdSettingsWrite->setEnabled(true);
      sbBdSettingsWrite->setMinimum(atof(bdSettings[cbIndex].GetAnswers()[0].first.c_str()));
      sbBdSettingsWrite->setMaximum(atof(bdSettings[cbIndex].GetAnswers()[1].first.c_str()));
      sbBdSettingsWrite->setSingleStep(atof(bdSettings[cbIndex].GetAnswers()[2].first.c_str()));
      sbBdSettingsWrite->setValue(00);
      sbBdSettingsWrite->setDecimals(0);
    }
    if( haha == ANSTYPE::FLOAT) sbBdSettingsWrite->setDecimals(3);
    //===== combo Box
    if( haha == ANSTYPE::COMBOX){
      cbBdAns->setEnabled(true);
      cbBdAns->clear();
      int ansIndex = -1;
      QString ans2 = "";
      for( int i = 0; i < (int) bdSettings[cbIndex].GetAnswers().size(); i++){
        cbBdAns->addItem(QString::fromStdString(bdSettings[cbIndex].GetAnswers()[i].second),
                        QString::fromStdString(bdSettings[cbIndex].GetAnswers()[i].first));

        if( ans == QString::fromStdString(bdSettings[cbIndex].GetAnswers()[i].first)) {
          ansIndex = i;
          ans2 = QString::fromStdString(bdSettings[cbIndex].GetAnswers()[i].second);
        }
      }
      cbBdAns->setCurrentIndex(ansIndex);
      leBdSettingsRead->setText( ans  + " [ " + ans2 + " ]");
      sbBdSettingsWrite->setEnabled(false);
      sbBdSettingsWrite->setStyleSheet("");
      sbBdSettingsWrite->setValue(0);
      leBdSettingsWrite->setEnabled(false);
      leBdSettingsWrite->setText(ans);
    }
    //===== lineEdit
    if( haha == ANSTYPE::STR || haha == ANSTYPE::BYTE || haha == ANSTYPE::BINARY){
      cbBdAns->clear();
      cbBdAns->setEnabled(false);
      leBdSettingsWrite->setEnabled(true);
      leBdSettingsWrite->clear();
      sbBdSettingsWrite->setEnabled(false);
      sbBdSettingsWrite->setStyleSheet("");
      sbBdSettingsWrite->setValue(0);
    }
  }else{
    cbBdAns->clear();
    cbBdAns->setEnabled(false);
    sbBdSettingsWrite->setEnabled(false);
  sbBdSettingsWrite->setStyleSheet("");
    sbBdSettingsWrite->cleanText();
    leBdSettingsWrite->setEnabled(false);
    leBdSettingsWrite->clear();
  }

  if(   bdSettings[cbIndex].GetPara() == PHA::DIG::StartSource.GetPara()
     || bdSettings[cbIndex].GetPara() == PHA::DIG::GlobalTriggerSource.GetPara() ){

    leBdSettingsWrite->setEnabled(true);
    leBdSettingsWrite->clear();
  }

  enableSignalSlot = true;
}

void DigiSettingsPanel::ReadChannelSetting(int cbIndex){

  if( enableSignalSlot == false ) return;
  enableSignalSlot = false;

  int ID = cbIQDigi->currentIndex();
  std::vector<Reg> chSettings ;

  if( digi[ID]->GetFPGAType() == DPPType::PHA ) {
    chSettings = PHA::CH::AllSettings;
  }else if( digi[ID]->GetFPGAType() == DPPType::PSD ) {
    chSettings = PSD::CH::AllSettings;
  }else{
    enableSignalSlot = true;
    return;
  }
  
  QString type;
  switch (chSettings[cbIndex].ReadWrite()) {
    case RW::ReadOnly : type ="Read Only"; break;
    case RW::WriteOnly : type ="Write Only"; break;
    case RW::ReadWrite : type ="Read/Write"; break;
  }
  leChSettingsType->setText(type);

  QString ans = QString::fromStdString(digi[ID]->ReadValue(chSettings[cbIndex], cbIQCh->currentData().toInt()));
  ANSTYPE haha = chSettings[cbIndex].GetAnswerType();

  if( haha == ANSTYPE::BYTE){
    leChSettingsRead->setText( "0x" + QString::number(ans.toULong(), 16).rightJustified(16, '0'));
  }else if( haha == ANSTYPE::BINARY ){
    leChSettingsRead->setText( "0b" + QString::number(ans.toUInt(), 2).rightJustified(18, '0'));
  }else{
    leChSettingsRead->setText(ans);
  }

  leChSettingsUnit->setText(QString::fromStdString(chSettings[cbIndex].GetUnit()));


  if( chSettings[cbIndex].ReadWrite() != RW::ReadOnly && haha != ANSTYPE::NONE ){

    if( haha == ANSTYPE::FLOAT || haha == ANSTYPE::INTEGER ){
      cbChSettingsWrite->clear();
      cbChSettingsWrite->setEnabled(false);
      leChSettingsWrite->setEnabled(false);
      leChSettingsWrite->clear();
      sbChSettingsWrite->setEnabled(true);
      sbChSettingsWrite->setMinimum(atof(chSettings[cbIndex].GetAnswers()[0].first.c_str()));
      sbChSettingsWrite->setMaximum(atof(chSettings[cbIndex].GetAnswers()[1].first.c_str()));
      sbChSettingsWrite->setSingleStep(atof(chSettings[cbIndex].GetAnswers()[2].first.c_str()));
      sbChSettingsWrite->setValue(ans.toFloat());
      sbChSettingsWrite->setDecimals(3);
    }
    if( haha == ANSTYPE::INTEGER) sbBdSettingsWrite->setDecimals(0);
    if( haha == ANSTYPE::COMBOX){
      cbChSettingsWrite->setEnabled(true);
      cbChSettingsWrite->clear();
      int ansIndex = -1;
      QString ans2 = "";
      for( int i = 0; i < (int) chSettings[cbIndex].GetAnswers().size(); i++){
        cbChSettingsWrite->addItem(QString::fromStdString(chSettings[cbIndex].GetAnswers()[i].second),
                        QString::fromStdString(chSettings[cbIndex].GetAnswers()[i].first));
        
        if( ans == QString::fromStdString(chSettings[cbIndex].GetAnswers()[i].first)) {
          ansIndex = i;
          ans2 = QString::fromStdString(chSettings[cbIndex].GetAnswers()[i].second);
        }      
      }
      cbChSettingsWrite->setCurrentIndex(ansIndex);
      leChSettingsRead->setText( ans  + " [ " + ans2 + " ]");
      sbChSettingsWrite->setEnabled(false);
      sbChSettingsWrite->setStyleSheet("");
      sbChSettingsWrite->setValue(0);
      leChSettingsWrite->setEnabled(false);
      leChSettingsWrite->setText(ans2);
    }
    if( haha == ANSTYPE::STR || haha == ANSTYPE::BYTE || haha == ANSTYPE::BINARY){
      cbChSettingsWrite->clear();
      cbChSettingsWrite->setEnabled(false);
      leChSettingsWrite->setEnabled(true);
      leChSettingsWrite->clear();
      sbChSettingsWrite->setEnabled(false);
      sbChSettingsWrite->setStyleSheet("");
      sbChSettingsWrite->setValue(0);
    }
  }else{
    cbChSettingsWrite->clear();
    cbChSettingsWrite->setEnabled(false);
    sbChSettingsWrite->setEnabled(false);
    sbChSettingsWrite->setStyleSheet("");
    sbChSettingsWrite->cleanText();
    leChSettingsWrite->setEnabled(false);
    leChSettingsWrite->clear();
  }

  enableSignalSlot = true;
}

bool DigiSettingsPanel::CheckDigitizersCanCopy(){
  int digiFromIndex = cbCopyDigiFrom->currentIndex();
  int digiToIndex = cbCopyDigiTo->currentIndex();
  if( digi[digiFromIndex]->GetModelName() != digi[digiToIndex]->GetModelName() ) return false;
  if( digi[digiFromIndex]->GetFPGAType() != digi[digiToIndex]->GetFPGAType() ) return false;
  if( digi[digiFromIndex]->GetFPGAVersion() != digi[digiToIndex]->GetFPGAVersion() ) return false;
  return true;
}

void DigiSettingsPanel::CheckRadioAndCheckedButtons(){

  int chFromIndex = -1;
  for( int i = 0 ; i < MaxNumberOfChannel ; i++){
    rbCopyChFrom[i]->setStyleSheet("");
    if( rbCopyChFrom[i]->isChecked() && cbCopyDigiFrom->currentIndex() == cbCopyDigiTo->currentIndex()){
      chFromIndex = i;
      rbCopyChFrom[i]->setStyleSheet("color : red;");
      chkChTo[i]->setChecked(false);
      chkChTo[i]->setStyleSheet("");
    }
  }

  for( int i = 0 ; i < MaxNumberOfChannel ; i++)  chkChTo[i]->setEnabled(true);
  if( chFromIndex >= 0 && cbCopyDigiFrom->currentIndex() == cbCopyDigiTo->currentIndex() ) chkChTo[chFromIndex]->setEnabled(false);

  bool isToIndexCleicked = false;
  for( int i = 0 ; i < MaxNumberOfChannel ; i++){
    if( chkChTo[i]->isChecked() ){
      isToIndexCleicked = true;
      chkChTo[i]->setStyleSheet("color : blue;");
    }else{
      chkChTo[i]->setStyleSheet("");
    }
  }

  pbCopyChannel->setEnabled(chFromIndex >= 0 && isToIndexCleicked );
}

bool DigiSettingsPanel::CopyChannelSettings(int digiFrom, int chFrom, int digiTo, int chTo){

  if( !CheckDigitizersCanCopy() ) return false;

  SendLogMsg("Copy Settings from DIG:" +  QString::number(digi[digiFrom]->GetSerialNumber()) + ", CH:" +  QString::number(chFrom) + " ---> DIG:" + QString::number(digi[digiTo]->GetSerialNumber()) + ", CH:" +  QString::number(chTo));
  for( int k = 0; k < (int) PHA::CH::AllSettings.size(); k ++){
    if( PHA::CH::AllSettings[k].ReadWrite() != RW::ReadWrite ) continue;
    std::string haha = digi[digiFrom]->GetSettingValueFromMemory(PHA::CH::AllSettings[k], chFrom);
    if( !digi[digiTo]->WriteValue( PHA::CH::AllSettings[k],  haha , chTo ) ){
      SendLogMsg("something wrong when copying setting : " + QString::fromStdString( PHA::CH::AllSettings[k].GetPara())) ;
      return false;
      break;
    }
  }
  return true;
}

bool DigiSettingsPanel::CopyBoardSettings(){
  int digiFromIndex = cbCopyDigiFrom->currentIndex();
  int digiToIndex = cbCopyDigiTo->currentIndex();

  if( !CheckDigitizersCanCopy() ) return false;

  SendLogMsg("Copy Settings from DIG:" +  QString::number(digi[digiFromIndex]->GetSerialNumber()) + " to DIG:" +  QString::number(digi[digiToIndex]->GetSerialNumber()));
  for( int i = 0; i < MaxNumberOfChannel; i++){
  if( chkChTo[i]->isChecked() ){
    //Copy setting
    for( int k = 0; k < (int) PHA::DIG::AllSettings.size(); k ++){
      if( PHA::DIG::AllSettings[k].ReadWrite() != RW::ReadWrite ) continue;
        if( ! digi[digiToIndex]->WriteValue( PHA::DIG::AllSettings[k],  digi[digiFromIndex]->GetSettingValueFromMemory(PHA::DIG::AllSettings[k])) ){
          SendLogMsg("something wrong when copying setting : " + QString::fromStdString( PHA::DIG::AllSettings[k].GetPara())) ;
          return false;
          break;
        }
      }
    }
  }
  SendLogMsg("------ done");
  return true;
}

bool DigiSettingsPanel::CopyWholeDigitizer(){
  if( !CopyBoardSettings() ) return false;

  int digiTo =  cbCopyDigiTo->currentIndex();

  for( int ch = 0; ch < digi[ID]->GetNChannels() ; ch ++){
    if ( ! CopyChannelSettings(cbCopyDigiFrom->currentIndex(), ch, digiTo, ch ) ) return false;
  }

  return true;
}
