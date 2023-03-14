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

DigiSettingsPanel::DigiSettingsPanel(Digitizer2Gen ** digi, unsigned short nDigi, QWidget * parent) : QWidget(parent){

  qDebug() << "DigiSettingsPanel constructor";

  setWindowTitle("Digitizers Settings");
  setGeometry(0, 0, 1850, 1000);
  //setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);


  this->digi = digi;
  this->nDigi = nDigi;
  if( nDigi > MaxNumberOfDigitizer ) {
    this->nDigi = MaxNumberOfChannel;
    qDebug() << "Please increase the MaxNumberOfChannel";
  }

  ID = 0;
  enableSignalSlot = false;

  QVBoxLayout * mainLayout = new QVBoxLayout(this); this->setLayout(mainLayout);
  QTabWidget * tabWidget = new QTabWidget(this); mainLayout->addWidget(tabWidget);
  connect(tabWidget, &QTabWidget::currentChanged, this, [=](int index){ ID = index;});

  //@========================== Tab for each digitizer
  for(unsigned short iDigi = 0; iDigi < this->nDigi; iDigi++){

    QScrollArea * scrollArea = new QScrollArea(this); 
    scrollArea->setWidgetResizable(true);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    tabWidget->addTab(scrollArea, "Digi-" + QString::number(digi[iDigi]->GetSerialNumber()));

    QWidget * tab = new QWidget(tabWidget);
    scrollArea->setWidget(tab);
    
    QHBoxLayout * tabLayout_H  = new QHBoxLayout(tab); //tab->setLayout(tabLayout_H);

    QVBoxLayout * tabLayout_V1 = new QVBoxLayout(); tabLayout_H->addLayout(tabLayout_V1);
    QVBoxLayout * tabLayout_V2 = new QVBoxLayout(); tabLayout_H->addLayout(tabLayout_V2);

    {//^====================== Group of Digitizer Info
      QGroupBox * infoBox = new QGroupBox("Board Info", tab);
      //infoBox->setSizePolicy(sizePolicy);

      QGridLayout * infoLayout = new QGridLayout(infoBox);
      tabLayout_V1->addWidget(infoBox);
      
      const unsigned short nRow = 4;
      for( unsigned short j = 0; j < (unsigned short) infoIndex.size(); j++){
        QLabel * lab = new QLabel(QString::fromStdString(infoIndex[j].first), tab);
        lab->setAlignment(Qt::AlignRight | Qt::AlignCenter);
        leInfo[iDigi][j] = new QLineEdit(tab);
        leInfo[iDigi][j]->setReadOnly(true);
        leInfo[iDigi][j]->setText(QString::fromStdString(digi[iDigi]->ReadValue(infoIndex[j].second)));
        infoLayout->addWidget(lab, j%nRow, 2*(j/nRow));
        infoLayout->addWidget(leInfo[iDigi][j], j%nRow, 2*(j/nRow) +1);
      }
    }

    {//^====================== Group Board status
      QGroupBox * statusBox = new QGroupBox("Board Status", tab);
      QGridLayout * statusLayout = new QGridLayout(statusBox);
      statusLayout->setAlignment(Qt::AlignLeft);
      statusLayout->setHorizontalSpacing(0);

      tabLayout_V1->addWidget(statusBox);

      //------- LED Status
      QLabel * lbLED = new QLabel("LED status : ");
      lbLED->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      statusLayout->addWidget(lbLED, 0, 0);

      for( int i = 0; i < 19; i++){
        LEDStatus[iDigi][i] = new QPushButton(tab);
        LEDStatus[iDigi][i]->setEnabled(false);
        LEDStatus[iDigi][i]->setFixedSize(QSize(30,30));
        LEDStatus[iDigi][i]->setToolTip(QString::number(i) + " - " + LEDToolTip[i]);
        LEDStatus[iDigi][i]->setToolTipDuration(-1);
        //TODO set tooltip position on top
        statusLayout->addWidget(LEDStatus[iDigi][i], 0, 1 + 19 - i);
      
      }

      //------- ACD Status
      QLabel * lbACQ = new QLabel("ACQ status : ");
      lbACQ->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      statusLayout->addWidget(lbACQ, 1, 0);

      for( int i = 0; i < 7; i++){
        ACQStatus[iDigi][i] = new QPushButton(tab);
        ACQStatus[iDigi][i]->setEnabled(false);
        ACQStatus[iDigi][i]->setFixedSize(QSize(30,30));
        ACQStatus[iDigi][i]->setToolTip(QString::number(i) + " - " + ACQToolTip[i]);
        ACQStatus[iDigi][i]->setToolTipDuration(-1);
        statusLayout->addWidget(ACQStatus[iDigi][i], 1, 1 + 7 - i);
      }

      //------- Temperatures
      QLabel * lbTemp = new QLabel("ADC Temperature [C] : ");
      lbTemp->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      statusLayout->addWidget(lbTemp, 2, 0);

      for( int i = 0; i < 8; i++){
        leTemp[iDigi][i] = new QLineEdit(tab);
        leTemp[iDigi][i]->setReadOnly(true);
        leTemp[iDigi][i]->setAlignment(Qt::AlignHCenter);
        statusLayout->addWidget(leTemp[iDigi][i], 2, 1 + 2*i, 1, 2);
      }

      for( int i = 0; i < statusLayout->columnCount(); i++) statusLayout->setColumnStretch(i, 0 );
    }

    {//^====================== Board Setting Buttons
      QGridLayout * bnLayout = new QGridLayout();
      tabLayout_V1->addLayout(bnLayout);
    
      int rowId = 0;
      //-------------------------------------
      QLabel * lbSettingFile = new QLabel("Setting File : ", tab);
      lbSettingFile->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      bnLayout->addWidget(lbSettingFile, rowId, 0);

      leSettingFile[iDigi] = new QLineEdit(tab);
      leSettingFile[iDigi]->setReadOnly(true);
      leSettingFile[iDigi]->setText(QString::fromStdString(digi[iDigi]->GetSettingFileName()));
      bnLayout->addWidget(leSettingFile[iDigi], rowId, 1, 1, 9);
      
      //-------------------------------------
      rowId ++;
      bnReadSettngs[iDigi] = new QPushButton("Refresh Settings", tab);
      bnLayout->addWidget(bnReadSettngs[iDigi], rowId, 0, 1, 2);
      connect(bnReadSettngs[iDigi], &QPushButton::clicked, this, &DigiSettingsPanel::RefreshSettings);
      
      bnResetBd[iDigi] = new QPushButton("Reset Board", tab);
      bnLayout->addWidget(bnResetBd[iDigi], rowId, 2, 1, 2);
      connect(bnResetBd[iDigi], &QPushButton::clicked, this, [=](){
         SendLogMsg("Reset Digitizer-" + QString::number(digi[ID]->GetSerialNumber()));
         digi[ID]->Reset();    
      });
      
      bnDefaultSetting[iDigi] = new QPushButton("Set Default PHA Settings", tab);
      bnLayout->addWidget(bnDefaultSetting[iDigi], rowId, 4, 1, 2);
      connect(bnDefaultSetting[iDigi], &QPushButton::clicked, this, &DigiSettingsPanel::SetDefaultPHASettigns);

      bnSaveSettings[iDigi] = new QPushButton("Save Settings", tab);
      bnLayout->addWidget(bnSaveSettings[iDigi], rowId, 6, 1, 2);
      connect(bnSaveSettings[iDigi], &QPushButton::clicked, this, &DigiSettingsPanel::SaveSettings);

      bnLoadSettings[iDigi] = new QPushButton("Load Settings", tab);
      bnLayout->addWidget(bnLoadSettings[iDigi], rowId, 8, 1, 2);
      connect(bnLoadSettings[iDigi], &QPushButton::clicked, this, &DigiSettingsPanel::LoadSettings);

      //---------------------------------------
      rowId ++;
      bnClearData[iDigi] = new QPushButton("Clear Data", tab);
      bnLayout->addWidget(bnClearData[iDigi], rowId, 0, 1, 2);
      connect(bnClearData[iDigi], &QPushButton::clicked, this, [=](){ digi[ID]->SendCommand(PHA::DIG::ClearData); });
      
      bnArmACQ[iDigi] = new QPushButton("Arm ACQ", tab);
      bnLayout->addWidget(bnArmACQ[iDigi], rowId, 2, 1, 2);
      connect(bnArmACQ[iDigi], &QPushButton::clicked, this, [=](){ digi[ID]->SendCommand(PHA::DIG::ArmACQ); });
      
      bnDisarmACQ[iDigi] = new QPushButton("Disarm ACQ", tab);
      bnLayout->addWidget(bnDisarmACQ[iDigi], rowId, 4, 1, 2);
      connect(bnDisarmACQ[iDigi], &QPushButton::clicked, this, [=](){ digi[ID]->SendCommand(PHA::DIG::DisarmACQ); });

      bnSoftwareStart[iDigi] = new QPushButton("Software Start ACQ", tab);
      bnLayout->addWidget(bnSoftwareStart[iDigi], rowId, 6, 1, 2);
      connect(bnSoftwareStart[iDigi], &QPushButton::clicked, this, [=](){ digi[ID]->SendCommand(PHA::DIG::SoftwareStartACQ); });

      bnSoftwareStop[iDigi] = new QPushButton("Software Stop ACQ", tab);
      bnLayout->addWidget(bnSoftwareStop[iDigi], rowId, 8, 1, 2);
      connect(bnSoftwareStop[iDigi], &QPushButton::clicked, this, [=](){ digi[ID]->SendCommand(PHA::DIG::SoftwareStopACQ); });


      //--------------- 
      if( digi[iDigi]->IsDummy() ){
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

    {//^====================== Group Board settings
      digiBox = new QGroupBox("Board Settings", tab);
      //digiBox->setSizePolicy(sizePolicy);
      QGridLayout * boardLayout = new QGridLayout(digiBox);
      tabLayout_V1->addWidget(digiBox);
        
      int rowId = 0;
      //-------------------------------------
      SetupComboBox(cbbClockSource[iDigi], PHA::DIG::ClockSource, -1, true, "Clock Source :", boardLayout, rowId, 0, 1, 2);

      //-------------------------------------
      rowId ++;
      QLabel * lbStartSource = new QLabel("Start Source :", tab);
      lbStartSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbStartSource, rowId, 0);

      for( int i = 0; i < (int) PHA::DIG::StartSource.GetAnswers().size(); i++){
        ckbStartSource[iDigi][i] = new QCheckBox( QString::fromStdString((PHA::DIG::StartSource.GetAnswers())[i].second), tab);
        boardLayout->addWidget(ckbStartSource[iDigi][i], rowId, 1 + i);
        connect(ckbStartSource[iDigi][i], &QCheckBox::stateChanged, this, &DigiSettingsPanel::SetStartSource);
      }

      //-------------------------------------
      rowId ++;
      QLabel * lbGlobalTrgSource = new QLabel("Global Trigger Source :", tab);
      lbGlobalTrgSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbGlobalTrgSource, rowId, 0);

      for( int i = 0; i < (int) PHA::DIG::GlobalTriggerSource.GetAnswers().size(); i++){
        ckbGlbTrgSource[iDigi][i] = new QCheckBox( QString::fromStdString((PHA::DIG::GlobalTriggerSource.GetAnswers())[i].second), tab);
        boardLayout->addWidget(ckbGlbTrgSource[iDigi][i], rowId, 1 + i);
        connect(ckbGlbTrgSource[iDigi][i], &QCheckBox::stateChanged, this, &DigiSettingsPanel::SetGlobalTriggerSource);
      }

      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbTrgOut[iDigi], PHA::DIG::TrgOutMode, -1, true, "Trg-OUT Mode :", boardLayout, rowId, 0, 1, 2);
            
      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbGPIO[iDigi], PHA::DIG::GPIOMode, -1, true, "GPIO Mode :", boardLayout, rowId, 0, 1, 2);

      //-------------------------------------
      QLabel * lbAutoDisarmAcq = new QLabel("Auto disarm ACQ :", tab);
      lbAutoDisarmAcq->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbAutoDisarmAcq, rowId, 4, 1, 2);

      cbbAutoDisarmAcq[iDigi] = new RComboBox(tab);
      boardLayout->addWidget(cbbAutoDisarmAcq[iDigi], rowId, 6);
      SetupShortComboBox(cbbAutoDisarmAcq[iDigi], PHA::DIG::EnableAutoDisarmACQ);
      connect(cbbAutoDisarmAcq[iDigi], &RComboBox::currentIndexChanged, this, [=](){
        if( !enableSignalSlot ) return;
        //printf("%s %d  %s \n", para.GetPara().c_str(), ch_index, cbb->currentData().toString().toStdString().c_str());
        QString msg;
        msg = QString::fromStdString(PHA::DIG::EnableAutoDisarmACQ.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
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
      SetupComboBox(cbbBusyIn[iDigi], PHA::DIG::BusyInSource, -1, true, "Busy In Source :", boardLayout, rowId, 0, 1, 2);

      QLabel * lbStatEvents = new QLabel("Stat. Event :", tab);
      lbStatEvents->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbStatEvents, rowId, 4, 1, 2);

      cbbStatEvents[iDigi] = new RComboBox(tab);
      boardLayout->addWidget(cbbStatEvents[iDigi], rowId, 6);
      SetupShortComboBox(cbbStatEvents[iDigi], PHA::DIG::EnableStatisticEvents);
      connect(cbbStatEvents[iDigi], &RComboBox::currentIndexChanged, this, [=](){
        if( !enableSignalSlot ) return;
        QString msg;
        msg = QString::fromStdString(PHA::DIG::EnableStatisticEvents.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
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
      SetupComboBox(cbbSyncOut[iDigi], PHA::DIG::SyncOutMode, -1, true, "Sync Out mode :", boardLayout, rowId, 0, 1, 2);

      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbBoardVetoSource[iDigi], PHA::DIG::BoardVetoSource, -1, true, "Board Veto Source :", boardLayout, rowId, 0, 1, 2);

      QLabel * lbBdVetoWidth = new QLabel("Board Veto Width [ns] :", tab);
      lbBdVetoWidth->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbBdVetoWidth, rowId, 3, 1, 2);

      dsbBdVetoWidth[iDigi] = new RSpinBox(tab, 0); // may be QDoubleSpinBox
      dsbBdVetoWidth[iDigi]->setMinimum(0);
      dsbBdVetoWidth[iDigi]->setMaximum(34359738360);
      dsbBdVetoWidth[iDigi]->setSingleStep(20);
      boardLayout->addWidget(dsbBdVetoWidth[iDigi], rowId, 5);
      connect(dsbBdVetoWidth[iDigi], &RSpinBox::valueChanged, this, [=](){
        if( !enableSignalSlot ) return;
        dsbBdVetoWidth[ID]->setStyleSheet("color:blue;");
      });
      connect(dsbBdVetoWidth[iDigi], &RSpinBox::returnPressed, this, [=](){
        if( !enableSignalSlot ) return;
        //printf("%s %d  %d \n", para.GetPara().c_str(), ch_index, spb->value());
        QString msg;
        msg = QString::fromStdString(PHA::DIG::BoardVetoWidth.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
        msg += " = " + QString::number(dsbBdVetoWidth[iDigi]->value());
        if( digi[ID]->WriteValue(PHA::DIG::BoardVetoWidth, std::to_string(dsbBdVetoWidth[iDigi]->value()), -1) ){
          dsbBdVetoWidth[ID]->setStyleSheet("");
          SendLogMsg(msg + "|OK.");
        }else{
          dsbBdVetoWidth[ID]->setStyleSheet("color:red;");
          SendLogMsg(msg + "|Fail.");
        }
      });

      cbbBdVetoPolarity[iDigi] = new RComboBox(tab);
      boardLayout->addWidget(cbbBdVetoPolarity[iDigi], rowId, 6);
      SetupShortComboBox(cbbBdVetoPolarity[iDigi], PHA::DIG::BoardVetoPolarity);
      
      //-------------------------------------
      rowId ++;
      SetupSpinBox(spbRunDelay[iDigi], PHA::DIG::RunDelay, -1, false, "Run Delay [ns] :", boardLayout, rowId, 0);

      //-------------------------------------
      QLabel * lbClockOutDelay = new QLabel("Temp. Clock Out Delay [ps] :", tab);
      lbClockOutDelay->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbClockOutDelay, rowId, 3, 1, 2);

      dsbVolatileClockOutDelay[iDigi] = new RSpinBox(tab, 3);
      dsbVolatileClockOutDelay[iDigi]->setMinimum(-18888.888);
      dsbVolatileClockOutDelay[iDigi]->setMaximum(18888.888);
      dsbVolatileClockOutDelay[iDigi]->setSingleStep(74.074);
      dsbVolatileClockOutDelay[iDigi]->setValue(0);
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
        msg = QString::fromStdString(PHA::DIG::VolatileClockOutDelay.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
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
      SetupComboBox(cbbIOLevel[iDigi], PHA::DIG::IO_Level, -1, true, "IO Level :", boardLayout, rowId, 0, 1, 2);

      QLabel * lbClockOutDelay2 = new QLabel("Perm. Clock Out Delay [ps] :", tab);
      lbClockOutDelay2->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbClockOutDelay2, rowId, 3, 1, 2);

      dsbClockOutDelay[iDigi] = new RSpinBox(tab, 3);
      dsbClockOutDelay[iDigi]->setMinimum(-18888.888);
      dsbClockOutDelay[iDigi]->setMaximum(18888.888);
      dsbClockOutDelay[iDigi]->setValue(0);
      dsbClockOutDelay[iDigi]->setSingleStep(74.074);
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
        msg = QString::fromStdString(PHA::DIG::PermanentClockOutDelay.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
        msg += " = " + QString::number(dsbClockOutDelay[iDigi]->value());
        if( digi[ID]->WriteValue(PHA::DIG::PermanentClockOutDelay, std::to_string(dsbClockOutDelay[ID]->value()), -1) ){
          dsbClockOutDelay[ID]->setStyleSheet("");
          SendLogMsg(msg + "|OK.");
        }else{
          dsbClockOutDelay[ID]->setStyleSheet("color:red;");
          SendLogMsg(msg + "|Fail.");
        }
      });
    }
    
    {//^====================== Test Pulse settings
      testPulseBox = new QGroupBox("Test Pulse Settings", tab); 
      tabLayout_V1->addWidget(testPulseBox);
      QGridLayout * testPulseLayout = new QGridLayout(testPulseBox);
      testPulseLayout->setAlignment(Qt::AlignLeft);
      testPulseLayout->setVerticalSpacing(0);

      SetupSpinBox(dsbTestPuslePeriod[iDigi], PHA::DIG::TestPulsePeriod, -1, false, "Period [ns] :", testPulseLayout, 0, 0);
      SetupSpinBox(dsbTestPusleWidth[iDigi], PHA::DIG::TestPulseWidth, -1, false, "Width [ns] :", testPulseLayout, 0, 2);
      SetupSpinBox(spbTestPusleLowLevel[iDigi], PHA::DIG::TestPulseLowLevel, -1, false, "Low Lvl. [LSB] :", testPulseLayout, 0, 4);
      SetupSpinBox(spbTestPusleHighLevel[iDigi], PHA::DIG::TestPulseHighLevel, -1, false, "High Lvl. [LSB] :", testPulseLayout, 0, 6);

      dsbTestPuslePeriod[iDigi]->setFixedSize(110, 30);
      dsbTestPuslePeriod[iDigi]->setDecimals(0);
      dsbTestPusleWidth[iDigi]->setFixedSize(110, 30);
      dsbTestPusleWidth[iDigi]->setDecimals(0);

      for( int i = 0; i < testPulseLayout->columnCount(); i++) testPulseLayout->setColumnStretch(i, 0 );
    }

    {//^====================== VGA settings
      VGABox = new QGroupBox("Gain Amplifier Settings", tab); 
      tabLayout_V1->addWidget(VGABox);
      QGridLayout * vgaLayout = new QGridLayout(VGABox);
      vgaLayout->setVerticalSpacing(0);
      //vgaLayout->setAlignment(Qt::AlignLeft);

      for( int k = 0; k < 4; k ++){
        QLabel * lb = new QLabel("VGA-" + QString::number(k) + " [dB] :", tab);
        lb->setAlignment(Qt::AlignRight | Qt::AlignCenter);
        vgaLayout->addWidget(lb, 0, 2*k);

        VGA[iDigi][k] = new RSpinBox(tab, 1);
        VGA[iDigi][k]->setMinimum(0);
        VGA[iDigi][k]->setMaximum(40);
        VGA[iDigi][k]->setSingleStep(0.5);
        vgaLayout->addWidget(VGA[iDigi][k], 0, 2*k+1);
        connect(VGA[iDigi][k], &RSpinBox::valueChanged, this, [=](){ 
          if( !enableSignalSlot ) return; 
          VGA[ID][k]->setStyleSheet("color:blue;");
        });
        connect(VGA[iDigi][k], &RSpinBox::returnPressed, this, [=](){
          if( !enableSignalSlot ) return;
          //printf("%s %d  %d \n", para.GetPara().c_str(), ch_index, spb->value());
          double step = VGA[ID][k]->singleStep();
          double value = VGA[ID][k]->value();
          VGA[ID][k]->setValue( (std::round(value/step) * step) );
          QString msg;
          msg = QString::fromStdString(PHA::VGA::VGAGain.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
          if( PHA::VGA::VGAGain.GetType() == TYPE::VGA ) msg += ",VGA:" + QString::number(k);
          msg += " = " + QString::number(VGA[ID][k]->value());
          if( digi[ID]->WriteValue(PHA::VGA::VGAGain, std::to_string(VGA[ID][k]->value()), k)){
            VGA[ID][k]->setStyleSheet("");
            SendLogMsg(msg + "|OK.");
          }else{
            VGA[ID][k]->setStyleSheet("color:red;");
            SendLogMsg(msg + "|Fail.");
          }
        });
      }
    }

    if( digi[iDigi]->GetFPGATyep() != "DPP_PHA" ) VGABox->setEnabled(false);

    {//^====================== Group channel settings
      QGroupBox * chBox = new QGroupBox("Channel Settings", tab); 
      //chBox->setSizePolicy(sizePolicy);
      tabLayout_V2->addWidget(chBox);
      QGridLayout * chLayout = new QGridLayout(chBox); //chBox->setLayout(chLayout);

      QTabWidget * chTabWidget = new QTabWidget(tab); chLayout->addWidget(chTabWidget);

      {//@.......... All Settings tab
        QWidget * tab_All = new QWidget(tab); 
        //tab_All->setStyleSheet("background-color: #EEEEEE");
        chTabWidget->addTab(tab_All, "All/Single Ch.");

        QGridLayout * allLayout = new QGridLayout(tab_All);
        allLayout->setAlignment(Qt::AlignTop);

        unsigned short ch = digi[iDigi]->GetNChannels();

        {//*--------- Group 0
          box0 = new QGroupBox("Channel Selection", tab);
          allLayout->addWidget(box0);
          QGridLayout * layout0 = new QGridLayout(box0);
          layout0->setAlignment(Qt::AlignLeft);

          QLabel * lbCh = new QLabel("Channel :", tab);
          lbCh->setAlignment(Qt::AlignCenter | Qt::AlignRight);
          layout0->addWidget(lbCh, 0, 0);

          cbChPick[iDigi] = new RComboBox(tab);
          cbChPick[iDigi]->addItem("All", -1);
          for( int i = 0; i < ch; i++) cbChPick[iDigi]->addItem("Ch-" + QString::number(i), i);
          layout0->addWidget(cbChPick[iDigi], 0, 1);
          connect(cbChPick[iDigi], &RComboBox::currentIndexChanged, this, [=](){
            int index = cbChPick[ID]->currentData().toInt();
            if(index == -1) {
              ShowSettingsToPanel();
              return;
            }else{
              enableSignalSlot = false;
              unsigned short ch = digi[iDigi]->GetNChannels();
              printf("index = %d, ch = %d\n", index, ch);
              FillComboBoxValueFromMemory(cbbOnOff[ID][ch], PHA::CH::ChannelEnable, index);
              FillSpinBoxValueFromMemory(spbDCOffset[ID][ch], PHA::CH::DC_Offset, index);
              FillSpinBoxValueFromMemory(spbThreshold[ID][ch], PHA::CH::TriggerThreshold, index);
              FillComboBoxValueFromMemory(cbbParity[ID][ch], PHA::CH::Polarity, index);
              FillSpinBoxValueFromMemory(spbRecordLength[ID][ch], PHA::CH::RecordLength, index);
              FillSpinBoxValueFromMemory(spbPreTrigger[ID][ch], PHA::CH::PreTrigger, index);
              FillSpinBoxValueFromMemory(spbInputRiseTime[ID][ch], PHA::CH::TimeFilterRiseTime, index);
              FillSpinBoxValueFromMemory(spbTriggerGuard[ID][ch], PHA::CH::TimeFilterRetriggerGuard, index);
              FillComboBoxValueFromMemory(cbbLowFilter[ID][ch], PHA::CH::EnergyFilterLowFreqFilter, index);
              FillComboBoxValueFromMemory(cbbWaveSource[ID][ch], PHA::CH::WaveDataSource, index);
              FillComboBoxValueFromMemory(cbbWaveRes[ID][ch], PHA::CH::WaveResolution, index);
              FillComboBoxValueFromMemory(cbbWaveSave[ID][ch], PHA::CH::WaveSaving, index);

              FillSpinBoxValueFromMemory(spbTrapRiseTime[ID][ch], PHA::CH::EnergyFilterRiseTime, index);
              FillSpinBoxValueFromMemory(spbTrapFlatTop[ID][ch], PHA::CH::EnergyFilterFlatTop, index);
              FillSpinBoxValueFromMemory(spbTrapPoleZero[ID][ch], PHA::CH::EnergyFilterPoleZero, index);
              FillSpinBoxValueFromMemory(spbPeaking[ID][ch], PHA::CH::EnergyFilterPeakingPosition, index);
              FillComboBoxValueFromMemory(cbbPeakingAvg[ID][ch], PHA::CH::EnergyFilterPeakingAvg, index);
              FillComboBoxValueFromMemory(cbbBaselineAvg[ID][ch], PHA::CH::EnergyFilterBaselineAvg, index);
              FillSpinBoxValueFromMemory(spbFineGain[ID][ch], PHA::CH::EnergyFilterFineGain, index);
              FillSpinBoxValueFromMemory(spbBaselineGuard[ID][ch], PHA::CH::EnergyFilterBaselineGuard, index);
              FillSpinBoxValueFromMemory(spbPileupGuard[ID][ch], PHA::CH::EnergyFilterPileUpGuard, index);

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
    
              enableSignalSlot = true;
            }
          });

        }

        int rowID = 0;
        {//*--------- Group 1
          box1 = new QGroupBox("Input Settings", tab);
          allLayout->addWidget(box1);
          QGridLayout * layout1 = new QGridLayout(box1);

          rowID = 0;
          SetupComboBox(cbbOnOff[iDigi][ch], PHA::CH::ChannelEnable, -1, true, "On/Off", layout1, rowID, 0);
          SetupComboBox(cbbWaveSource[iDigi][ch], PHA::CH::WaveDataSource, -1, true, "Wave Data Source", layout1, rowID, 2);

          rowID ++;
          SetupComboBox(cbbWaveRes[iDigi][ch], PHA::CH::WaveResolution, -1, true,  "Wave Resol.", layout1, rowID, 0);
          SetupComboBox(cbbWaveSave[iDigi][ch], PHA::CH::WaveSaving, -1, true, "Wave Save", layout1, rowID, 2);

          rowID ++;
          SetupComboBox(cbbParity[iDigi][ch], PHA::CH::Polarity, -1, true, "Parity", layout1, rowID, 0);
          SetupComboBox(cbbLowFilter[iDigi][ch], PHA::CH::EnergyFilterLowFreqFilter, -1, true, "Low Freq. Filter", layout1, rowID, 2);

          rowID ++;
          SetupSpinBox(spbDCOffset[iDigi][ch], PHA::CH::DC_Offset, -1, false, "DC Offset [%]", layout1, rowID, 0);
          SetupSpinBox(spbThreshold[iDigi][ch], PHA::CH::TriggerThreshold, -1, false, "Threshold [LSB]", layout1, rowID, 2);

          rowID ++;
          SetupSpinBox(spbInputRiseTime[iDigi][ch], PHA::CH::TimeFilterRiseTime, -1, false, "Input Rise Time [ns]", layout1, rowID, 0);
          SetupSpinBox(spbTriggerGuard[iDigi][ch], PHA::CH::TimeFilterRetriggerGuard, -1, false, "Trigger Guard [ns]", layout1, rowID, 2);

          rowID ++;
          SetupSpinBox(spbRecordLength[iDigi][ch], PHA::CH::RecordLength, -1, false, "Record Length [ns]", layout1, rowID, 0);
          SetupSpinBox(spbPreTrigger[iDigi][ch], PHA::CH::PreTrigger, -1, false, "Pre Trigger [ns]", layout1, rowID, 2);

        }

        {//*--------- Group 3
          box3 = new QGroupBox("Trap. Settings", tab);
          allLayout->addWidget(box3);
          QGridLayout * layout3 = new QGridLayout(box3);

          //------------------------------
          rowID = 0;    
          SetupSpinBox(spbTrapRiseTime[iDigi][ch], PHA::CH::EnergyFilterRiseTime, -1, false, "Trap. Rise Time [ns]", layout3, rowID, 0);
          SetupSpinBox(spbTrapFlatTop[iDigi][ch], PHA::CH::EnergyFilterFlatTop, -1, false, "Trap. Flat Top [ns]", layout3, rowID, 2);
          SetupSpinBox(spbTrapPoleZero[iDigi][ch], PHA::CH::EnergyFilterPoleZero, -1, false, "Trap. Pole Zero [ns]", layout3, rowID, 4);
          
          //------------------------------
          rowID ++;
          SetupSpinBox(spbPeaking[iDigi][ch], PHA::CH::EnergyFilterPeakingPosition, -1, false, "Peaking [%]", layout3, rowID, 0);
          SetupSpinBox(spbBaselineGuard[iDigi][ch], PHA::CH::EnergyFilterBaselineGuard, -1, false, "Baseline Guard [ns]", layout3, rowID, 2);
          SetupSpinBox(spbPileupGuard[iDigi][ch], PHA::CH::EnergyFilterPileUpGuard, -1, false, "Pile-up Guard [ns]", layout3, rowID, 4);
          
          //------------------------------
          rowID ++;
          SetupComboBox(cbbPeakingAvg[iDigi][ch], PHA::CH::EnergyFilterPeakingAvg, -1, true, "Peak Avg", layout3, rowID, 0);
          SetupComboBox(cbbBaselineAvg[iDigi][ch], PHA::CH::EnergyFilterBaselineAvg, -1, true, "Baseline Avg", layout3, rowID, 2);
          SetupSpinBox(spbFineGain[iDigi][ch], PHA::CH::EnergyFilterFineGain, -1, false, "Fine Gain", layout3, rowID, 4);
          
        }

        {//*--------- Group 4
          box4 = new QGroupBox("Probe Settings", tab);
          allLayout->addWidget(box4);
          QGridLayout * layout4 = new QGridLayout(box4);

          //------------------------------
          rowID = 0;
          SetupComboBox(cbbAnaProbe0[iDigi][ch], PHA::CH::WaveAnalogProbe0, -1, true, "Analog Prob. 0", layout4, rowID, 0, 1, 2);
          SetupComboBox(cbbAnaProbe1[iDigi][ch], PHA::CH::WaveAnalogProbe1, -1, true, "Analog Prob. 1", layout4, rowID, 3, 1, 2);        

          //------------------------------
          rowID ++;
          SetupComboBox(cbbDigProbe0[iDigi][ch], PHA::CH::WaveDigitalProbe0, -1, true, "Digitial Prob. 0", layout4, rowID, 0, 1, 2);
          SetupComboBox(cbbDigProbe1[iDigi][ch], PHA::CH::WaveDigitalProbe1, -1, true, "Digitial Prob. 1", layout4, rowID, 3, 1, 2);

          //------------------------------
          rowID ++;
          SetupComboBox(cbbDigProbe2[iDigi][ch], PHA::CH::WaveDigitalProbe2, -1, true, "Digitial Prob. 2", layout4, rowID, 0, 1, 2);
          SetupComboBox(cbbDigProbe3[iDigi][ch], PHA::CH::WaveDigitalProbe3, -1, true, "Digitial Prob. 3", layout4, rowID, 3, 1, 2);
          
        }

        {//*--------- Group 5
          box5 = new QGroupBox("Trigger Settings", tab);
          allLayout->addWidget(box5);
          QGridLayout * layout5 = new QGridLayout(box5);

          //------------------------------
          rowID = 0;
          SetupComboBox(cbbEvtTrigger[iDigi][ch], PHA::CH::EventTriggerSource, -1, true, "Event Trig. Source", layout5, rowID, 0);
          SetupComboBox(cbbWaveTrigger[iDigi][ch], PHA::CH::WaveTriggerSource, -1, true, "Wave Trig. Source", layout5, rowID, 2);

          //------------------------------
          rowID ++;
          SetupComboBox(cbbChVetoSrc[iDigi][ch], PHA::CH::ChannelVetoSource, -1, true, "Veto Source", layout5, rowID, 0);

          //------------------------------
          rowID ++;
          SetupComboBox(cbbCoinMask[iDigi][ch], PHA::CH::CoincidenceMask, -1, true, "Coin. Mask", layout5, rowID, 0);
          SetupComboBox(cbbAntiCoinMask[iDigi][ch], PHA::CH::AntiCoincidenceMask, -1, true, "Anti-Coin. Mask", layout5, rowID, 2);

          //------------------------------
          rowID ++;
          SetupSpinBox(spbCoinLength[iDigi][ch], PHA::CH::CoincidenceLength, -1, false, "Coin. Length [ns]", layout5, rowID, 0);
          SetupSpinBox(spbADCVetoWidth[iDigi][ch], PHA::CH::ADCVetoWidth, -1, false, "ADC Veto Length [ns]", layout5, rowID, 2);

          for( int i = 0; i < layout5->columnCount(); i++) layout5->setColumnStretch(i, 1);

        }

        {//*--------- Group 6
          box6 = new QGroupBox("Other Settings", tab);
          allLayout->addWidget(box6);
          QGridLayout * layout6 = new QGridLayout(box6);

          //------------------------------
          rowID = 0 ;
          SetupComboBox(cbbEventSelector[iDigi][ch], PHA::CH::EventSelector, -1, true, "Event Selector", layout6, rowID, 0);
          SetupComboBox(cbbWaveSelector[iDigi][ch], PHA::CH::WaveSelector, -1, true, "Wave Selector", layout6, rowID, 2);

          //------------------------------
          rowID ++;
          SetupSpinBox(spbEnergySkimLow[iDigi][ch], PHA::CH::EnergySkimLowDiscriminator, -1, false, "Energy Skim Low", layout6, rowID, 0);
          SetupSpinBox(spbEnergySkimHigh[iDigi][ch], PHA::CH::EnergySkimHighDiscriminator, -1, false, "Energy Skim High", layout6, rowID, 2);
        }
      }

      {//@============== Status  tab
        QTabWidget * statusTab = new QTabWidget(tab);
        chTabWidget->addTab(statusTab, "Status");

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
      
      
      {//@============== input  tab
        inputTab = new QTabWidget(tab);
        chTabWidget->addTab(inputTab, "Input");

        SetupComboBoxTab(cbbOnOff, PHA::CH::ChannelEnable, "On/Off", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbDCOffset, PHA::CH::DC_Offset, "DC Offset [%]", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbThreshold, PHA::CH::TriggerThreshold, "Threshold [LSB]", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbParity, PHA::CH::Polarity, "Parity", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbRecordLength, PHA::CH::RecordLength, "Record Length [ns]", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbPreTrigger, PHA::CH::PreTrigger, "PreTrigger [ns]", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbInputRiseTime, PHA::CH::TimeFilterRiseTime, "Input Rise Time [ns]", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbTriggerGuard, PHA::CH::TimeFilterRetriggerGuard, "Trigger Guard [ns]", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbLowFilter, PHA::CH::EnergyFilterLowFreqFilter, "Low Freq. Filter", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbWaveSource, PHA::CH::WaveDataSource, "Wave Data Dource", inputTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbWaveRes, PHA::CH::WaveResolution,  "Wave Resol.", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbWaveSave, PHA::CH::WaveSaving, "Wave Save", inputTab, iDigi, digi[iDigi]->GetNChannels());

        for( int ch = 0; ch < digi[iDigi]->GetNChannels(); ch++){
          //Set color of some combox
          cbbOnOff[iDigi][ch]->setItemData(1, QBrush(Qt::green), Qt::ForegroundRole);
          connect(cbbOnOff[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](int index){ cbbOnOff[ID][ch]->setStyleSheet(index == 1 ? "color : green;" : "");});
          cbbParity[iDigi][ch]->setItemData(1, QBrush(Qt::green), Qt::ForegroundRole);
          connect(cbbParity[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](int index){ cbbParity[ID][ch]->setStyleSheet(index == 1 ?  "color : green;" : "");});
          cbbLowFilter[iDigi][ch]->setItemData(1, QBrush(Qt::green), Qt::ForegroundRole);
          connect(cbbLowFilter[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](int index){ cbbLowFilter[ID][ch]->setStyleSheet(index == 1 ? "color : green;": "");});
        }

      }
      
      {//@============== Trap  tab
        trapTab = new QTabWidget(tab);
        chTabWidget->addTab(trapTab, "Trapezoid");

        SetupSpinBoxTab(spbTrapRiseTime, PHA::CH::EnergyFilterRiseTime, "Trap. Rise Time [ns]", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbTrapFlatTop, PHA::CH::EnergyFilterFlatTop, "Trap. Flat Top [ns]", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbTrapPoleZero, PHA::CH::EnergyFilterPoleZero, "Trap. Pole Zero [ns]", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbPeaking, PHA::CH::EnergyFilterPeakingPosition, "Peaking [%]", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbPeakingAvg, PHA::CH::EnergyFilterPeakingAvg, "Peak Avg.", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbBaselineAvg, PHA::CH::EnergyFilterBaselineAvg, "Baseline Avg.", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbFineGain, PHA::CH::EnergyFilterFineGain, "Fine Gain", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbBaselineGuard, PHA::CH::EnergyFilterBaselineGuard, "Baseline Guard [ns]", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbPileupGuard, PHA::CH::EnergyFilterPileUpGuard, "Pile-up Guard [ns]", trapTab, iDigi, digi[iDigi]->GetNChannels());
      }

      {//@============== Probe  tab
        probeTab = new QTabWidget(tab);
        chTabWidget->addTab(probeTab, "Probe");

        SetupComboBoxTab(cbbAnaProbe0, PHA::CH::WaveAnalogProbe0, "Analog Prob. 0", probeTab, iDigi, digi[iDigi]->GetNChannels(), 4);
        SetupComboBoxTab(cbbAnaProbe1, PHA::CH::WaveAnalogProbe1, "Analog Prob. 1", probeTab, iDigi, digi[iDigi]->GetNChannels(), 4);
        SetupComboBoxTab(cbbDigProbe0, PHA::CH::WaveDigitalProbe0, "Digital Prob. 0", probeTab, iDigi, digi[iDigi]->GetNChannels(), 4);
        SetupComboBoxTab(cbbDigProbe1, PHA::CH::WaveDigitalProbe1, "Digital Prob. 1", probeTab, iDigi, digi[iDigi]->GetNChannels(), 4);
        SetupComboBoxTab(cbbDigProbe2, PHA::CH::WaveDigitalProbe2, "Digital Prob. 2", probeTab, iDigi, digi[iDigi]->GetNChannels(), 4);
        SetupComboBoxTab(cbbDigProbe3, PHA::CH::WaveDigitalProbe3, "Digital Prob. 3", probeTab, iDigi, digi[iDigi]->GetNChannels(), 4);
      }

      {//@============== Other  tab
        otherTab = new QTabWidget(tab);
        chTabWidget->addTab(otherTab, "Others");

        SetupComboBoxTab(cbbEventSelector, PHA::CH::EventSelector, "Event Selector", otherTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbWaveSelector, PHA::CH::WaveSelector, "Wave Selector", otherTab, iDigi, digi[iDigi]->GetNChannels(), 2 );
        SetupSpinBoxTab(spbEnergySkimLow, PHA::CH::EnergySkimLowDiscriminator, "Energy Skim Low", otherTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbEnergySkimHigh, PHA::CH::EnergySkimHighDiscriminator, "Energy Skim High", otherTab, iDigi, digi[iDigi]->GetNChannels());
      }

      {//@============== Trigger  tab
        triggerTab = new QTabWidget(tab);
        chTabWidget->addTab(triggerTab, "Trigger");

        SetupComboBoxTab(cbbEvtTrigger, PHA::CH::EventTriggerSource, "Event Trig. Source", triggerTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbWaveTrigger, PHA::CH::WaveTriggerSource, "Wave Trig. Source", triggerTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbChVetoSrc, PHA::CH::ChannelVetoSource, "Veto Source", triggerTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbCoinMask, PHA::CH::CoincidenceMask, "Coin. Mask", triggerTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbAntiCoinMask, PHA::CH::AntiCoincidenceMask, "Anti-Coin. Mask", triggerTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbCoinLength, PHA::CH::CoincidenceLength, "Coin. Length [ns]", triggerTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbADCVetoWidth, PHA::CH::ADCVetoWidth, "ADC Veto Length [ns]", triggerTab, iDigi, digi[iDigi]->GetNChannels());
      }

      for( int ch = 0; ch < digi[ID]->GetNChannels() + 1; ch++) {
        //send UpdateScopeSetting signal
        connect(spbDCOffset[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(spbRecordLength[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(spbPreTrigger[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(spbThreshold[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(spbTrapRiseTime[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(spbTrapFlatTop[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(spbTrapPoleZero[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(spbPeaking[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(spbTriggerGuard[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(spbTrapRiseTime[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(spbFineGain[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(spbBaselineGuard[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(spbPileupGuard[iDigi][ch], &RSpinBox::returnPressed, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(cbbParity[iDigi][ch], &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(cbbWaveRes[iDigi][ch], &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(cbbPeakingAvg[iDigi][ch], &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(cbbLowFilter[iDigi][ch], &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(cbbBaselineAvg[iDigi][ch], &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::UpdateScopeSetting);

        connect(cbbAnaProbe0[iDigi][ch], &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(cbbAnaProbe1[iDigi][ch], &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(cbbDigProbe0[iDigi][ch], &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(cbbDigProbe1[iDigi][ch], &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(cbbDigProbe2[iDigi][ch], &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::UpdateScopeSetting);
        connect(cbbDigProbe3[iDigi][ch], &RComboBox::currentIndexChanged, this, &DigiSettingsPanel::UpdateScopeSetting);
        
        //----- SyncBox
        connect(cbbOnOff[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbOnOff, ch);});
        connect(spbDCOffset[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbDCOffset, ch);});
        connect(spbThreshold[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbThreshold, ch);});
        connect(cbbParity[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbParity, ch);});
        connect(spbRecordLength[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbRecordLength, ch);});
        connect(spbPreTrigger[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbPreTrigger, ch);});
        connect(spbInputRiseTime[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbInputRiseTime, ch);});
        connect(spbTriggerGuard[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbTriggerGuard, ch);});
        connect(cbbLowFilter[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbLowFilter, ch);});
        connect(cbbWaveSource[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveSource, ch);});
        connect(cbbWaveRes[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveRes, ch);});
        connect(cbbWaveSave[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveSave, ch);});

        connect(spbTrapRiseTime[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbTrapRiseTime, ch);});
        connect(spbTrapFlatTop[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbTrapFlatTop, ch);});
        connect(spbTrapPoleZero[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbTrapPoleZero, ch);});
        connect(spbPeaking[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbPeaking, ch);});
        connect(cbbPeakingAvg[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbPeakingAvg, ch);});
        connect(cbbBaselineAvg[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbBaselineAvg, ch);});
        connect(spbFineGain[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbFineGain, ch);});
        spbFineGain[iDigi][ch]->setSingleStep(0.001);
        spbFineGain[iDigi][ch]->setDecimals(3);

        connect(spbBaselineGuard[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbBaselineGuard, ch);});
        connect(spbPileupGuard[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbPileupGuard, ch);});
        
        connect(cbbAnaProbe0[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbAnaProbe0, ch);});
        connect(cbbAnaProbe1[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbAnaProbe1, ch);});
        connect(cbbDigProbe0[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe0, ch);});
        connect(cbbDigProbe1[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe1, ch);});
        connect(cbbDigProbe2[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe2, ch);});
        connect(cbbDigProbe3[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe3, ch);});

        connect(cbbEventSelector[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbEventSelector, ch);});
        connect(cbbWaveSelector[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveSelector, ch);});
        connect(spbEnergySkimLow[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbEnergySkimLow, ch);});
        connect(spbEnergySkimHigh[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbEnergySkimHigh, ch);});

        connect(cbbEvtTrigger[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbEvtTrigger, ch);});
        connect(cbbWaveTrigger[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveTrigger, ch);});
        connect(cbbChVetoSrc[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbChVetoSrc, ch);});
        connect(cbbCoinMask[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbCoinMask, ch);});
        connect(cbbAntiCoinMask[iDigi][ch], &RComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbAntiCoinMask, ch);});
        connect(spbCoinLength[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbCoinLength, ch);});
        connect(spbADCVetoWidth[iDigi][ch], &RSpinBox::returnPressed, this, [=](){ SyncSpinBox(spbADCVetoWidth, ch);});
      }

      
      {//@============== Trigger Mask/Map  tab

        //TODO==========================
        triggerMapTab = new QTabWidget(tab);
        chTabWidget->addTab(triggerMapTab, "Trigger Map");

        QGridLayout * triggerLayout = new QGridLayout(triggerMapTab);
        triggerLayout->setAlignment(Qt::AlignTop);

        int rowID = 0;
        //----------------------------
        SetupComboBox( cbAllEvtTrigger, PHA::CH::EventTriggerSource, -1, false, "Event Trigger Source (all ch.)", triggerLayout, rowID, 0);
        SetupComboBox( cbAllWaveTrigger, PHA::CH::WaveTriggerSource, -1, false, "Wave Trigger Source (all ch.)", triggerLayout, rowID, 2);

        //----------------------------
        rowID ++;
        SetupComboBox( cbAllCoinMask, PHA::CH::CoincidenceMask, -1, false, "Coincident Mask (all ch.)", triggerLayout, rowID, 0);
        SetupSpinBox( sbAllCoinLength, PHA::CH::CoincidenceLength, -1, false, "Coincident Length [ns] (all ch.)", triggerLayout, rowID, 2);


        QSignalMapper * triggerMapper = new QSignalMapper(tab);
        connect(triggerMapper, &QSignalMapper::mappedInt, this, &DigiSettingsPanel::onTriggerClick);

        //----------------------------
        rowID ++;
        QGroupBox * triggerBox = new QGroupBox("Trigger Mask", tab);
        triggerLayout->addWidget(triggerBox, rowID, 0, 1, 4);

        QGridLayout * tbLayout = new QGridLayout(triggerBox);
        tbLayout->setAlignment(Qt::AlignCenter);
        tbLayout->setSpacing(0);

        //----------------------------
        rowID = 0;
        QLabel * instr = new QLabel("Reading: Column (C) represents a trigger channel for Row (R) channel.\nFor example, R3C1 = ch-3 trigger source is ch-1.\n", tab);
        instr->setAlignment(Qt::AlignLeft);
        tbLayout->addWidget(instr, rowID, 0, 1, 64+15);

        rowID ++;
        int colID = 0;
        for(int i = 0; i < digi[iDigi]->GetNChannels(); i++){
          colID = 1;

          if( i % 4 == 0){
            QLabel * lllba = new QLabel(QString::number(i), tab);
            lllba->setAlignment(Qt::AlignTop | Qt::AlignRight);
            tbLayout->addWidget(lllba, i + 2 + i/4, 0, 4, 1); 
          }

          if( i == 0 ){
            for( int j = 0; j < digi[iDigi]->GetNChannels(); j++){
              if( j % 4 == 0) {
                QLabel * lllb = new QLabel(QString::number(j), tab);
                lllb->setAlignment(Qt::AlignLeft);
                tbLayout->addWidget(lllb, rowID, 1 + j + j/4, 1, 4); 
              }
            }
            rowID ++;
          }

          for(int j = 0; j < digi[iDigi]->GetNChannels(); j++){

            trgMap[i][j] = new QPushButton(tab);
            trgMap[i][j]->setFixedSize(QSize(10,10));
            trgMapClickStatus[i][j] = false;
            tbLayout->addWidget(trgMap[i][j], rowID, colID);

            triggerMapper->setMapping(trgMap[i][j], (iDigi << 16) + (i << 8) + j);
            connect(trgMap[i][j], SIGNAL(clicked()), triggerMapper, SLOT(map()));

            colID ++;
            if( j%4 == 3 && j!= digi[iDigi]->GetNChannels() - 1){
              QFrame * vSeparator = new QFrame(tab);
              vSeparator->setFrameShape(QFrame::VLine);
              tbLayout->addWidget(vSeparator, rowID, colID);
              colID++;
            }
          }

          rowID++;
          if( i%4 == 3 && i != digi[iDigi]->GetNChannels() - 1){
            QFrame * hSeparator = new QFrame(tab);
            hSeparator->setFrameShape(QFrame::HLine);
            tbLayout->addWidget(hSeparator, rowID, 1, 1, digi[iDigi]->GetNChannels() + 15);
            rowID++;
          }
        }
      }

    } //=== end of channel group

  } //=== end of tab

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
        enableSignalSlot = true;
      });

      cbBdSettings = new RComboBox(ICTab);
      for( int i = 0; i < (int) PHA::DIG::AllSettings.size(); i++ ){
        cbBdSettings->addItem( QString::fromStdString( PHA::DIG::AllSettings[i].GetPara() ), i);
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
      leBdSettingsRead->setFixedWidth(200);
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
        msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
        msg += " = " + cbBdAns->currentData().toString();
        if( digi[ID]->WriteValue(para, value) ){
          leBdSettingsRead->setText( QString::fromStdString(digi[ID]->GetSettingValue(para)));
          SendLogMsg(msg + "|OK.");
          cbBdAns->setStyleSheet("");
          ShowSettingsToPanel();
        }else{
          leBdSettingsRead->setText("fail write value");
          SendLogMsg(msg + "|Fail.");
          cbBdAns->setStyleSheet("color:red;");
        }
      });

      sbBdSettingsWrite = new RSpinBox(ICTab);
      sbBdSettingsWrite->setFixedWidth(200);
      inquiryLayout->addWidget(sbBdSettingsWrite, rowID, 7);
      connect(sbBdSettingsWrite, &RSpinBox::valueChanged, this, [=](){ sbBdSettingsWrite->setStyleSheet("color: green;");});
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
        msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
        msg += " = " + QString::number(sbBdSettingsWrite->value());
        if( digi[ID]->WriteValue(para, std::to_string(sbBdSettingsWrite->value()))){
          leBdSettingsRead->setText( QString::fromStdString(digi[ID]->GetSettingValue(para)));
          SendLogMsg(msg + "|OK.");
          sbBdSettingsWrite->setStyleSheet("");
          ShowSettingsToPanel();
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
      connect(leBdSettingsWrite, &QLineEdit::textChanged, this, [=](){leBdSettingsWrite->setStyleSheet("color: green;");});
      connect(leBdSettingsWrite, &QLineEdit::returnPressed, this, [=](){
        if( !enableSignalSlot ) return;
        std::string value = leBdSettingsWrite->text().toStdString();
        Reg para = PHA::DIG::AllSettings[cbBdSettings->currentIndex()];
        ID = cbIQDigi->currentIndex();
        QString msg;
        msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
        msg += " = " + QString::number(sbBdSettingsWrite->value());
        if( digi[ID]->WriteValue(para, value)){
          leBdSettingsRead->setText( QString::fromStdString(digi[ID]->GetSettingValue(para)));
          SendLogMsg(msg + "|OK.");
          sbBdSettingsWrite->setStyleSheet("");
          ShowSettingsToPanel();
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
      for( int i = 0; i < (int) PHA::CH::AllSettings.size(); i++ ){
        cbChSettings->addItem( QString::fromStdString( PHA::CH::AllSettings[i].GetPara() ), i);
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
      leChSettingsRead->setFixedWidth(200);
      leChSettingsRead->setReadOnly(true);
      inquiryLayout->addWidget(leChSettingsRead, rowID, 3);
      
      leChSettingsUnit = new QLineEdit(ICTab);
      leChSettingsUnit->setAlignment(Qt::AlignHCenter);
      leChSettingsUnit->setFixedWidth(50);
      leChSettingsUnit->setReadOnly(true);
      inquiryLayout->addWidget(leChSettingsUnit, rowID, 4);

      cbChAns = new RComboBox(ICTab);
      cbChAns->setFixedWidth(200);
      inquiryLayout->addWidget(cbChAns, rowID, 6);
      connect(cbChAns, &RComboBox::currentIndexChanged, this, [=](){
        if( !enableSignalSlot ) return;
        std::string value = cbChAns->currentData().toString().toStdString();
        leChSettingsWrite->setText(QString::fromStdString(value));
        leChSettingsWrite->setStyleSheet("");

        Reg para = PHA::CH::AllSettings[cbChSettings->currentIndex()];
        ID = cbIQDigi->currentIndex();
        int ch_index = cbIQCh->currentIndex();
        QString msg;
        msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
        msg += ",CH:" + QString::number(ch_index);
        msg += " = " + cbChAns->currentData().toString();
        if( digi[ID]->WriteValue(para, value, ch_index) ){
          leChSettingsRead->setText( QString::fromStdString(digi[ID]->GetSettingValue(para)));
          SendLogMsg(msg + "|OK.");
          cbChAns->setStyleSheet("");
          ShowSettingsToPanel();
        }else{
          leChSettingsRead->setText("fail write value");
          SendLogMsg(msg + "|Fail.");
          cbChAns->setStyleSheet("color:red;");
        }
      });

      sbChSettingsWrite = new RSpinBox(ICTab);
      sbChSettingsWrite->setFixedWidth(200);
      inquiryLayout->addWidget(sbChSettingsWrite, rowID, 7);
      connect(sbChSettingsWrite, &RSpinBox::valueChanged, this, [=](){ sbChSettingsWrite->setStyleSheet("color: green;");});
      connect(sbChSettingsWrite, &RSpinBox::returnPressed, this, [=](){ 
        if( !enableSignalSlot ) return;
        if( sbChSettingsWrite->decimals() == 0 && sbChSettingsWrite->singleStep() != 1) {
          double step = sbChSettingsWrite->singleStep();
          double value = sbChSettingsWrite->value();
          sbChSettingsWrite->setValue( (std::round(value/step) * step) );
        }

        Reg para = PHA::CH::AllSettings[cbBdSettings->currentIndex()];
        ID = cbIQDigi->currentIndex();
        int ch_index = cbIQCh->currentIndex();
        QString msg;
        msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
        msg += ",CH:" + QString::number(ch_index);
        msg += " = " + QString::number(sbChSettingsWrite->value());
        if( digi[ID]->WriteValue(para, std::to_string(sbChSettingsWrite->value()), ch_index)){
          leChSettingsRead->setText( QString::fromStdString(digi[ID]->GetSettingValue(para)));
          SendLogMsg(msg + "|OK.");
          sbChSettingsWrite->setStyleSheet("");
          ShowSettingsToPanel();
        }else{
          leChSettingsRead->setText("fail write value");
          SendLogMsg(msg + "|Fail.");
          sbChSettingsWrite->setStyleSheet("color:red;");
        }
      });

      leChSettingsWrite = new QLineEdit(ICTab);
      leChSettingsWrite->setFixedWidth(200);
      inquiryLayout->addWidget(leChSettingsWrite, rowID, 8);
      connect(leChSettingsWrite, &QLineEdit::textChanged, this, [=](){leChSettingsWrite->setStyleSheet("color: green;");});
      connect(leChSettingsWrite, &QLineEdit::returnPressed, this, [=](){
        if( !enableSignalSlot ) return;
        std::string value = leChSettingsWrite->text().toStdString();
        Reg para = PHA::CH::AllSettings[cbChSettings->currentIndex()];
        ID = cbIQDigi->currentIndex();
        int ch_index = cbIQCh->currentIndex();
        QString msg;
        msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
        msg += " = " + QString::number(sbChSettingsWrite->value());
        if( digi[ID]->WriteValue(para, value, ch_index)){
          leChSettingsRead->setText( QString::fromStdString(digi[ID]->GetSettingValue(para)));
          SendLogMsg(msg + "|OK.");
          sbChSettingsWrite->setStyleSheet("");
          ShowSettingsToPanel();
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

        ID = cbCopyDigiTo->currentIndex();
        ShowSettingsToPanel();
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

  EnableControl();

  cbBdSettings->setCurrentText("ModelName");
  cbChSettings->setCurrentIndex(10);

  show();

  enableSignalSlot = true;
}

DigiSettingsPanel::~DigiSettingsPanel(){

  printf("%s\n", __func__);

}

//^================================================================
void DigiSettingsPanel::onTriggerClick(int haha){
  
  unsigned short iDig = haha >> 16;
  unsigned short ch = (haha >> 8 ) & 0xFF;
  unsigned short ch2 = haha & 0xFF;

  qDebug() << "Digi-" << iDig << ", Ch-" << ch << ", " << ch2; 

  if(trgMapClickStatus[ch][ch2]){
    trgMap[ch][ch2]->setStyleSheet(""); 
    trgMapClickStatus[ch][ch2] = false;
  }else{
    trgMap[ch][ch2]->setStyleSheet("background-color: red;"); 
    trgMapClickStatus[ch][ch2] = true;
  }

  //format triggermask for ch;
  unsigned long mask = 0;
  for( int i = 0; i < digi[ID]->GetNChannels(); i++){
    if( trgMapClickStatus[ch][i] ) mask += (1 << i);
  }

  QString kaka = "0x"+QString::number(mask, 16);

  QString msg;
  msg = QString::fromStdString(PHA::CH::ChannelsTriggerMask.GetPara() ) + "|DIG:" + QString::number(digi[iDig]->GetNChannels()) + ",CH:" + QString::number(ch) + " = " + kaka;

  if( digi[iDig]->WriteValue(PHA::CH::ChannelsTriggerMask, kaka.toStdString(), ch) ){
    SendLogMsg(msg + "|OK.");
  }else{
    SendLogMsg(msg + "|Fail.");
    digi[iDig]->ReadValue(PHA::CH::ChannelsTriggerMask, ch);
    ShowSettingsToPanel();
  }

}

void DigiSettingsPanel::ReadTriggerMap(){

    enableSignalSlot = false;

    cbAllEvtTrigger->setCurrentIndex(cbbEvtTrigger[ID][MaxNumberOfChannel]->currentIndex());
    cbAllWaveTrigger->setCurrentIndex(cbbWaveTrigger[ID][MaxNumberOfChannel]->currentIndex());
    cbAllCoinMask->setCurrentIndex(cbbCoinMask[ID][MaxNumberOfChannel]->currentIndex());
    sbAllCoinLength->setValue(spbCoinLength[ID][MaxNumberOfChannel]->value());

    for( int ch = 0; ch < (int) digi[ID]->GetNChannels(); ch ++){
      unsigned long  mask = std::stoul(digi[ID]->GetSettingValue(PHA::CH::ChannelsTriggerMask, ch));

      for( int k = 0; k < (int) digi[ID]->GetNChannels(); k ++ ){
        trgMapClickStatus[ch][k] = ( (mask >> k) & 0x1 );
        if( (mask >> k) & 0x1 ){
          trgMap[ch][k]->setStyleSheet("background-color: red;"); 
        }else{
          trgMap[ch][k]->setStyleSheet("");
        }
      }
    }

    enableSignalSlot = true;
}

//^================================================================

void DigiSettingsPanel::RefreshSettings(){
  digi[ID]->ReadAllSettings();
  ShowSettingsToPanel();
}

void DigiSettingsPanel::EnableControl(){

  ShowSettingsToPanel();

  bool enable = !digi[ID]->IsAcqOn();

  digiBox->setEnabled(enable);
  if( digi[ID]->GetFPGATyep() == "DPP_PHA") VGABox->setEnabled(enable);
  if( ckbGlbTrgSource[ID][3]->isChecked() ) testPulseBox->setEnabled(enable);
  box1->setEnabled(enable);
  box3->setEnabled(enable);
  box4->setEnabled(enable);
  box5->setEnabled(enable);
  box6->setEnabled(enable);

  for( int i = 0; i < nDigi; i++){
    bnReadSettngs[i]->setEnabled(enable);
    bnResetBd[i]->setEnabled(enable);
    bnDefaultSetting[i]->setEnabled(enable);
    bnSaveSettings[i]->setEnabled(enable);
    bnLoadSettings[i]->setEnabled(enable);
    bnClearData[i]->setEnabled(enable);
    bnArmACQ[i]->setEnabled(enable);
    bnDisarmACQ[i]->setEnabled(enable);
    bnSoftwareStart[i]->setEnabled(enable);
    bnSoftwareStop[i]->setEnabled(enable);
  }

  QVector<QTabWidget*> tempArray = {inputTab, trapTab, probeTab, otherTab, triggerTab };

  for( int k = 0; k < tempArray.size(); k++){
    for( int i = 0; i < tempArray[k]->count(); i++) {
      QWidget* currentTab = tempArray[k]->widget(i);
      if( currentTab ){
        QList<QWidget*> childWidgets = currentTab->findChildren<QWidget*>();
        for(int j=0; j<childWidgets.count(); j++) {
            childWidgets[j]->setEnabled(enable);
        }
      }
    }
  }
  triggerMapTab->setEnabled(enable);

}

void DigiSettingsPanel::SaveSettings(){

  //TODO default file Path
  QString filePath = QFileDialog::getSaveFileName(this, "Save Settings File", "", "Data file (*.dat);;Text files (*.txt);;All files (*.*)");

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
        SendLogMsg("<font style=\"color:red;\"> Fail to write setting file.</font>");
      }; break;

      case -1 : {
        leSettingFile[ID]->setText("fail to save setting file, same settings are empty.");
        SendLogMsg("<font style=\"color:red;\"> Fail to save setting file, same settings are empty.</font>");
      }; break;
    };

  }

}

void DigiSettingsPanel::LoadSettings(){
  QFileDialog fileDialog(this);
  fileDialog.setFileMode(QFileDialog::ExistingFile);
  fileDialog.setNameFilter("Data file (*.dat);;Text file (*.txt);;All file (*.*)");
  fileDialog.exec();

  QString fileName = fileDialog.selectedFiles().at(0);

  leSettingFile[ID]->setText(fileName);
  //TODO ==== check is the file valid;

  if( digi[ID]->LoadSettingsFromFile(fileName.toStdString().c_str()) ){
    SendLogMsg("Loaded settings file " + fileName + " for Digi-" + QString::number(digi[ID]->GetSerialNumber()));
  }else{
    SendLogMsg("Fail to Loaded settings file " + fileName + " for Digi-" + QString::number(digi[ID]->GetSerialNumber()));
  }

  ShowSettingsToPanel();
}

void DigiSettingsPanel::SetDefaultPHASettigns(){
  SendLogMsg("Program Digitizer-" + QString::number(digi[ID]->GetSerialNumber()) + " to default PHA.");
  digi[ID]->ProgramPHA();
  RefreshSettings();
}

void DigiSettingsPanel::ShowSettingsToPanel(){

  enableSignalSlot = false;

  printf("%s Digi-%d\n", __func__, digi[ID]->GetSerialNumber());

  for (unsigned short j = 0; j < (unsigned short) infoIndex.size(); j++){
    leInfo[ID][j]->setText(QString::fromStdString(digi[ID]->GetSettingValue(infoIndex[j].second)));
  } 

  //--------- LED Status
  unsigned int ledStatus = atoi(digi[ID]->GetSettingValue(PHA::DIG::LED_status).c_str());
  for( int i = 0; i < 19; i++){
    if( (ledStatus >> i) & 0x1 ) {
      LEDStatus[ID][i]->setStyleSheet("background-color:green;");
    }else{
      LEDStatus[ID][i]->setStyleSheet("");
    }
  }

  //--------- ACQ Status
  unsigned int acqStatus = atoi(digi[ID]->GetSettingValue(PHA::DIG::ACQ_status).c_str());
  for( int i = 0; i < 7; i++){
    if( (acqStatus >> i) & 0x1 ) {
      ACQStatus[ID][i]->setStyleSheet("background-color:green;");
    }else{
      ACQStatus[ID][i]->setStyleSheet("");
    }
  }

  //-------- temperature
  for( int i = 0; i < 8; i++){
    leTemp[ID][i]->setText(QString::fromStdString(digi[ID]->GetSettingValue(PHA::DIG::TempSensADC[i])));
  }

  //-------- board settings
  FillComboBoxValueFromMemory(cbbClockSource[ID], PHA::DIG::ClockSource);

  QString result = QString::fromStdString(digi[ID]->GetSettingValue(PHA::DIG::StartSource));
  QStringList resultList = result.remove(QChar(' ')).split("|");
  //qDebug() << resultList << "," << resultList.count();
  for( int j = 0; j < (int) PHA::DIG::StartSource.GetAnswers().size(); j++){
    ckbStartSource[ID][j]->setChecked(false);
    for( int i = 0; i < resultList.count(); i++){
      //qDebug() << resultList[i] << ", " << QString::fromStdString((DIGIPARA::DIG::StartSource.GetAnswers())[j].first);
      if( resultList[i] == QString::fromStdString((PHA::DIG::StartSource.GetAnswers())[j].first) ) ckbStartSource[ID][j]->setChecked(true);
    }
  }

  result = QString::fromStdString(digi[ID]->GetSettingValue(PHA::DIG::GlobalTriggerSource));
  resultList = result.remove(QChar(' ')).split("|");
  testPulseBox->setEnabled(false);
  for( int j = 0; j < (int) PHA::DIG::StartSource.GetAnswers().size(); j++){
    ckbGlbTrgSource[ID][j]->setChecked(false);
    for( int i = 0; i < resultList.count(); i++){
      if( resultList[i] == QString::fromStdString((PHA::DIG::GlobalTriggerSource.GetAnswers())[j].first) ) {
        ckbGlbTrgSource[ID][j]->setChecked(true);
        if( resultList[i] == "TestPulse" ) testPulseBox->setEnabled(true);
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

  //@============================== Channel setting/ status
  for( int ch = 0; ch < digi[ID]->GetNChannels(); ch++){

    unsigned int status = atoi(digi[ID]->GetSettingValue(PHA::CH::ChannelStatus).c_str());
    for( int i = 0; i < 9; i++){
      if( (status >> i) & 0x1 ) {
        chStatus[ID][ch][i]->setStyleSheet("background-color:green;");
      }else{
        chStatus[ID][ch][i]->setStyleSheet("");
      }
    }

    chGainFactor[ID][ch]->setText(QString::fromStdString(digi[ID]->GetSettingValue(PHA::CH::GainFactor, ch)));
    chADCToVolts[ID][ch]->setText(QString::fromStdString(digi[ID]->GetSettingValue(PHA::CH::ADCToVolts, ch)));

    FillComboBoxValueFromMemory(cbbOnOff[ID][ch], PHA::CH::ChannelEnable, ch);
    FillSpinBoxValueFromMemory(spbDCOffset[ID][ch], PHA::CH::DC_Offset, ch);
    FillSpinBoxValueFromMemory(spbThreshold[ID][ch], PHA::CH::TriggerThreshold, ch);
    FillComboBoxValueFromMemory(cbbParity[ID][ch], PHA::CH::Polarity, ch);
    FillSpinBoxValueFromMemory(spbRecordLength[ID][ch], PHA::CH::RecordLength, ch);
    FillSpinBoxValueFromMemory(spbPreTrigger[ID][ch], PHA::CH::PreTrigger, ch);
    FillSpinBoxValueFromMemory(spbInputRiseTime[ID][ch], PHA::CH::TimeFilterRiseTime, ch);
    FillSpinBoxValueFromMemory(spbTriggerGuard[ID][ch], PHA::CH::TimeFilterRetriggerGuard, ch);
    FillComboBoxValueFromMemory(cbbLowFilter[ID][ch], PHA::CH::EnergyFilterLowFreqFilter, ch);
    FillComboBoxValueFromMemory(cbbWaveSource[ID][ch], PHA::CH::WaveDataSource, ch);
    FillComboBoxValueFromMemory(cbbWaveRes[ID][ch], PHA::CH::WaveResolution, ch);
    FillComboBoxValueFromMemory(cbbWaveSave[ID][ch], PHA::CH::WaveSaving, ch);

    FillSpinBoxValueFromMemory(spbTrapRiseTime[ID][ch], PHA::CH::EnergyFilterRiseTime, ch);
    FillSpinBoxValueFromMemory(spbTrapFlatTop[ID][ch], PHA::CH::EnergyFilterFlatTop, ch);
    FillSpinBoxValueFromMemory(spbTrapPoleZero[ID][ch], PHA::CH::EnergyFilterPoleZero, ch);
    FillSpinBoxValueFromMemory(spbPeaking[ID][ch], PHA::CH::EnergyFilterPeakingPosition, ch);
    FillComboBoxValueFromMemory(cbbPeakingAvg[ID][ch], PHA::CH::EnergyFilterPeakingAvg, ch);
    FillComboBoxValueFromMemory(cbbBaselineAvg[ID][ch], PHA::CH::EnergyFilterBaselineAvg, ch);
    FillSpinBoxValueFromMemory(spbFineGain[ID][ch], PHA::CH::EnergyFilterFineGain, ch);
    FillSpinBoxValueFromMemory(spbBaselineGuard[ID][ch], PHA::CH::EnergyFilterBaselineGuard, ch);
    FillSpinBoxValueFromMemory(spbPileupGuard[ID][ch], PHA::CH::EnergyFilterPileUpGuard, ch);

    FillComboBoxValueFromMemory(cbbAnaProbe0[ID][ch], PHA::CH::WaveAnalogProbe0, ch);
    FillComboBoxValueFromMemory(cbbAnaProbe1[ID][ch], PHA::CH::WaveAnalogProbe1, ch);
    FillComboBoxValueFromMemory(cbbDigProbe0[ID][ch], PHA::CH::WaveDigitalProbe0, ch);
    FillComboBoxValueFromMemory(cbbDigProbe1[ID][ch], PHA::CH::WaveDigitalProbe1, ch);
    FillComboBoxValueFromMemory(cbbDigProbe2[ID][ch], PHA::CH::WaveDigitalProbe2, ch);
    FillComboBoxValueFromMemory(cbbDigProbe3[ID][ch], PHA::CH::WaveDigitalProbe3, ch);

    FillComboBoxValueFromMemory(cbbEventSelector[ID][ch], PHA::CH::EventSelector, ch);
    FillComboBoxValueFromMemory(cbbWaveSelector[ID][ch], PHA::CH::WaveSelector, ch);
    FillSpinBoxValueFromMemory(spbEnergySkimLow[ID][ch], PHA::CH::EnergySkimLowDiscriminator, ch);
    FillSpinBoxValueFromMemory(spbEnergySkimHigh[ID][ch], PHA::CH::EnergySkimHighDiscriminator, ch);

    FillComboBoxValueFromMemory(cbbEvtTrigger[ID][ch], PHA::CH::EventTriggerSource, ch);
    FillComboBoxValueFromMemory(cbbWaveTrigger[ID][ch], PHA::CH::WaveTriggerSource, ch);
    FillComboBoxValueFromMemory(cbbChVetoSrc[ID][ch], PHA::CH::ChannelVetoSource, ch);
    FillComboBoxValueFromMemory(cbbCoinMask[ID][ch], PHA::CH::CoincidenceMask, ch);
    FillComboBoxValueFromMemory(cbbAntiCoinMask[ID][ch], PHA::CH::AntiCoincidenceMask, ch);
    FillSpinBoxValueFromMemory(spbCoinLength[ID][ch], PHA::CH::CoincidenceLength, ch);
    FillSpinBoxValueFromMemory(spbADCVetoWidth[ID][ch], PHA::CH::ADCVetoWidth, ch);

  }

  enableSignalSlot = true;

  if( cbChPick[ID]->currentData().toInt() >= 0 ) return;

  SyncComboBox(cbbOnOff, -1);
  SyncComboBox(cbbParity, -1);
  SyncComboBox(cbbLowFilter, -1);
  SyncComboBox(cbbWaveSource, -1);
  SyncComboBox(cbbWaveRes, -1);
  SyncComboBox(cbbWaveSave, -1);
  SyncComboBox(cbbPeakingAvg, -1);
  SyncComboBox(cbbBaselineAvg, -1);
  SyncComboBox(cbbAnaProbe0, -1);
  SyncComboBox(cbbAnaProbe1, -1);
  SyncComboBox(cbbDigProbe0, -1);
  SyncComboBox(cbbDigProbe1, -1);
  SyncComboBox(cbbDigProbe2, -1);
  SyncComboBox(cbbDigProbe3, -1);
  SyncComboBox(cbbEventSelector, -1);
  SyncComboBox(cbbWaveSelector , -1);
  SyncComboBox(cbbEvtTrigger   , -1);
  SyncComboBox(cbbWaveTrigger  , -1);
  SyncComboBox(cbbChVetoSrc    , -1);
  SyncComboBox(cbbCoinMask     , -1);
  SyncComboBox(cbbAntiCoinMask , -1);

  SyncSpinBox(spbDCOffset      , -1);
  SyncSpinBox(spbThreshold     , -1);
  SyncSpinBox(spbRecordLength  , -1);
  SyncSpinBox(spbPreTrigger    , -1);
  SyncSpinBox(spbInputRiseTime , -1);
  SyncSpinBox(spbTriggerGuard  , -1);
  SyncSpinBox(spbTrapRiseTime  , -1);
  SyncSpinBox(spbTrapFlatTop   , -1);
  SyncSpinBox(spbTrapPoleZero  , -1);
  SyncSpinBox(spbPeaking       , -1);
  SyncSpinBox(spbFineGain      , -1);
  SyncSpinBox(spbBaselineGuard , -1);
  SyncSpinBox(spbPileupGuard   , -1);
  SyncSpinBox(spbEnergySkimHigh, -1);
  SyncSpinBox(spbEnergySkimLow , -1);
  SyncSpinBox(spbCoinLength    , -1);
  SyncSpinBox(spbADCVetoWidth  , -1);

  ReadTriggerMap();

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
  msg = QString::fromStdString(PHA::DIG::StartSource.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
  msg += " = " + QString::fromStdString(value);
  SendLogMsg(msg);

  digi[ID]->WriteValue(PHA::DIG::StartSource, value);
}

void DigiSettingsPanel::SetGlobalTriggerSource(){
  if( !enableSignalSlot ) return;

  std::string value = "";
  testPulseBox->setEnabled(false);
  for( int i = 0; i < (int) PHA::DIG::GlobalTriggerSource.GetAnswers().size(); i++){
    if( ckbGlbTrgSource[ID][i]->isChecked() ){
      //printf("----- %s \n", DIGIPARA::DIG::StartSource.GetAnswers()[i].first.c_str());
      if( value != "" ) value += " | ";
      value += PHA::DIG::GlobalTriggerSource.GetAnswers()[i].first;
      if( PHA::DIG::GlobalTriggerSource.GetAnswers()[i].first == "TestPulse" ) testPulseBox->setEnabled(true);
    }
  }

  //printf("================ %s\n", value.c_str());
  QString msg;
  msg = QString::fromStdString(PHA::DIG::GlobalTriggerSource.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
  msg += " = " + QString::fromStdString(value);
  SendLogMsg(msg);

  digi[ID]->WriteValue(PHA::DIG::GlobalTriggerSource, value);

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
    msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
    if( para.GetType() == TYPE::CH ) msg += ",CH:" + (index == -1 ? "All" : QString::number(index));
    if( para.GetType() == TYPE::VGA ) msg += ",VGA:" + QString::number(index);
    msg += " = " + cbb->currentData().toString();
    if( digi[ID]->WriteValue(para, cbb->currentData().toString().toStdString(), index)){
      SendLogMsg(msg + "|OK.");
      cbb->setStyleSheet("");
      ShowSettingsToPanel();
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
    msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[ID]->GetSerialNumber());
    if( para.GetType() == TYPE::CH ) msg += ",CH:" + (index == -1 ? "All" : QString::number(index));
    msg += " = " + QString::number(spb->value());
    if( digi[ID]->WriteValue(para, std::to_string(spb->value()), index)){
      SendLogMsg(msg + "|OK.");
      spb->setStyleSheet("");
      ShowSettingsToPanel();
    }else{
      SendLogMsg(msg + "|Fail.");
      spb->setStyleSheet("color:red;");
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
  QString result = QString::fromStdString(digi[ID]->GetSettingValue(para, ch_index));
  //printf("%s === %s, %d, %p\n", __func__, result.toStdString().c_str(), ID, cbb);
  int index = cbb->findData(result);
  if( index >= 0 && index < cbb->count()) {
    cbb->setCurrentIndex(index);
  }else{
    //printf("%s  %s\n", para.GetPara().c_str(), result.toStdString().c_str());
  }
}

void DigiSettingsPanel::FillSpinBoxValueFromMemory(RSpinBox *&spb, const Reg para, int ch_index){
  QString result = QString::fromStdString(digi[ID]->GetSettingValue(para, ch_index));
  //printf("%s === %s, %d, %p\n", __func__, result.toStdString().c_str(), ID, spb);
  spb->setValue(result.toDouble());
}

void DigiSettingsPanel::ReadBoardSetting(int cbIndex){
  enableSignalSlot = false;

  QString type;
  switch (PHA::DIG::AllSettings[cbIndex].ReadWrite()) {
    case RW::ReadOnly : type ="Read Only"; break;
    case RW::WriteOnly : type ="Write Only"; break;
    case RW::ReadWrite : type ="Read/Write"; break;
  }
  leBdSettingsType->setText(type);

  QString ans = QString::fromStdString(digi[cbIQDigi->currentIndex()]->ReadValue(PHA::DIG::AllSettings[cbIndex]));
  ANSTYPE haha = PHA::DIG::AllSettings[cbIndex].GetAnswerType();

  if( haha == ANSTYPE::BYTE){
    leBdSettingsRead->setText( "0x" + QString::number(ans.toULong(), 16).rightJustified(16, '0'));
  }else if( haha == ANSTYPE::BINARY ){
    leBdSettingsRead->setText( "0b" + QString::number(ans.toUInt(), 2).rightJustified(18, '0'));
  }else{
    leBdSettingsRead->setText(ans);
  }
  leBdSettingsUnit->setText(QString::fromStdString(PHA::DIG::AllSettings[cbIndex].GetUnit()));

  if( PHA::DIG::AllSettings[cbIndex].ReadWrite() != RW::ReadOnly && haha != ANSTYPE::NONE ){

    //===== spin box
    if( haha == ANSTYPE::FLOAT || haha == ANSTYPE::INTEGER ){
      cbBdAns->clear();
      cbBdAns->setEnabled(false);
      leBdSettingsWrite->setEnabled(false);
      leBdSettingsWrite->clear();
      sbBdSettingsWrite->setEnabled(true);
      sbBdSettingsWrite->setMinimum(atof(PHA::DIG::AllSettings[cbIndex].GetAnswers()[0].first.c_str()));
      sbBdSettingsWrite->setMaximum(atof(PHA::DIG::AllSettings[cbIndex].GetAnswers()[1].first.c_str()));
      sbBdSettingsWrite->setSingleStep(atof(PHA::DIG::AllSettings[cbIndex].GetAnswers()[2].first.c_str()));
      sbBdSettingsWrite->setValue(00);
      sbBdSettingsWrite->setDecimals(0);
    }
    if( haha == ANSTYPE::FLOAT) sbBdSettingsWrite->setDecimals(3);
    //===== combo Box
    if( haha == ANSTYPE::LIST){
      cbBdAns->setEnabled(true);
      cbBdAns->clear();
      for( int i = 0; i < (int) PHA::DIG::AllSettings[cbIndex].GetAnswers().size(); i++){
        cbBdAns->addItem(QString::fromStdString(PHA::DIG::AllSettings[cbIndex].GetAnswers()[i].second),
                        QString::fromStdString(PHA::DIG::AllSettings[cbIndex].GetAnswers()[i].first));
      }
      sbBdSettingsWrite->setEnabled(false);
      sbBdSettingsWrite->setValue(0);
      leBdSettingsWrite->setEnabled(false);
      leBdSettingsWrite->clear();
    }
    //===== lineEdit
    if( haha == ANSTYPE::STR || haha == ANSTYPE::BYTE || haha == ANSTYPE::BINARY){
      cbBdAns->clear();
      cbBdAns->setEnabled(false);
      leBdSettingsWrite->setEnabled(true);
      leBdSettingsWrite->clear();
      sbBdSettingsWrite->setEnabled(false);
      sbBdSettingsWrite->setValue(0);
    }
  }else{
    cbBdAns->clear();
    cbBdAns->setEnabled(false);
    sbBdSettingsWrite->setEnabled(false);
    sbBdSettingsWrite->cleanText();
    leBdSettingsWrite->setEnabled(false);
    leBdSettingsWrite->clear();
  }

  if(   PHA::DIG::AllSettings[cbIndex].GetPara() == PHA::DIG::StartSource.GetPara()
     || PHA::DIG::AllSettings[cbIndex].GetPara() == PHA::DIG::GlobalTriggerSource.GetPara() ){

    leBdSettingsWrite->setEnabled(true);
    leBdSettingsWrite->clear();
  }

  enableSignalSlot = true;
}

void DigiSettingsPanel::ReadChannelSetting(int cbIndex){
  enableSignalSlot = false;

  QString type;
  switch (PHA::CH::AllSettings[cbIndex].ReadWrite()) {
    case RW::ReadOnly : type ="Read Only"; break;
    case RW::WriteOnly : type ="Write Only"; break;
    case RW::ReadWrite : type ="Read/Write"; break;
  }
  leChSettingsType->setText(type);

  QString ans = QString::fromStdString(digi[cbIQDigi->currentIndex()]->ReadValue(PHA::CH::AllSettings[cbIndex], cbIQCh->currentData().toInt()));
  ANSTYPE haha = PHA::CH::AllSettings[cbIndex].GetAnswerType();

  if( haha == ANSTYPE::BYTE){
    leChSettingsRead->setText( "0x" + QString::number(ans.toULong(), 16).rightJustified(16, '0'));
  }else if( haha == ANSTYPE::BINARY ){
    leChSettingsRead->setText( "0b" + QString::number(ans.toUInt(), 2).rightJustified(18, '0'));
  }else{
    leChSettingsRead->setText(ans);
  }

  leChSettingsUnit->setText(QString::fromStdString(PHA::CH::AllSettings[cbIndex].GetUnit()));


  if( PHA::CH::AllSettings[cbIndex].ReadWrite() != RW::ReadOnly && haha != ANSTYPE::NONE ){

    if( haha == ANSTYPE::FLOAT || haha == ANSTYPE::INTEGER ){
      cbChAns->clear();
      cbChAns->setEnabled(false);
      leChSettingsWrite->setEnabled(false);
      leChSettingsWrite->clear();
      sbChSettingsWrite->setEnabled(true);
      sbChSettingsWrite->setMinimum(atof(PHA::CH::AllSettings[cbIndex].GetAnswers()[0].first.c_str()));
      sbChSettingsWrite->setMaximum(atof(PHA::CH::AllSettings[cbIndex].GetAnswers()[1].first.c_str()));
      sbChSettingsWrite->setSingleStep(atof(PHA::CH::AllSettings[cbIndex].GetAnswers()[2].first.c_str()));
      sbChSettingsWrite->setValue(00);
      sbChSettingsWrite->setDecimals(0);
    }
    if( haha == ANSTYPE::FLOAT) sbBdSettingsWrite->setDecimals(3);
    if( haha == ANSTYPE::LIST){
      cbChAns->setEnabled(true);
      cbChAns->clear();
      for( int i = 0; i < (int) PHA::CH::AllSettings[cbIndex].GetAnswers().size(); i++){
        cbChAns->addItem(QString::fromStdString(PHA::CH::AllSettings[cbIndex].GetAnswers()[i].second),
                        QString::fromStdString(PHA::CH::AllSettings[cbIndex].GetAnswers()[i].first));
      }
      sbChSettingsWrite->setEnabled(false);
      sbChSettingsWrite->setValue(0);
      leChSettingsWrite->setEnabled(false);
      leChSettingsWrite->clear();
    }
    if( haha == ANSTYPE::STR || haha == ANSTYPE::BYTE || haha == ANSTYPE::BINARY){
      cbChAns->clear();
      cbChAns->setEnabled(false);
      leChSettingsWrite->setEnabled(true);
      leChSettingsWrite->clear();
      sbChSettingsWrite->setEnabled(false);
      sbChSettingsWrite->setValue(0);
    }
  }else{
    cbChAns->clear();
    cbChAns->setEnabled(false);
    sbChSettingsWrite->setEnabled(false);
    sbChSettingsWrite->cleanText();
    leChSettingsWrite->setEnabled(false);
    leChSettingsWrite->clear();
  }

  enableSignalSlot = true;
}

void DigiSettingsPanel::CheckRadioAndCheckedButtons(){

  int chFromIndex = -1;
  for( int i = 0 ; i < MaxNumberOfChannel ; i++){
    rbCopyChFrom[i]->setStyleSheet("");
    if( rbCopyChFrom[i]->isChecked() ){
      chFromIndex = i;
      rbCopyChFrom[i]->setStyleSheet("color : red;");
      chkChTo[i]->setChecked(false);
      chkChTo[i]->setStyleSheet("");
    }
  }

  for( int i = 0 ; i < MaxNumberOfChannel ; i++)  chkChTo[i]->setEnabled(true);
  if( chFromIndex >= 0 ) chkChTo[chFromIndex]->setEnabled(false);

  bool isToIndexCleicked = false;
  for( int i = 0 ; i < MaxNumberOfChannel ; i++){
    if( chkChTo[i]->isChecked() ){
      isToIndexCleicked = true;
      chkChTo[i]->setStyleSheet("color : red;");
    }else{
      chkChTo[i]->setStyleSheet("");
    }
  }

  pbCopyChannel->setEnabled(chFromIndex >= 0 && isToIndexCleicked );

}

bool DigiSettingsPanel::CopyChannelSettings(int digiFrom, int chFrom, int digiTo, int chTo){

  SendLogMsg("Copy Settings from DIG:" +  QString::number(digi[digiFrom]->GetSerialNumber()) + ", CH:" +  QString::number(chFrom) + " ---> DIG:" + QString::number(digi[digiTo]->GetSerialNumber()) + ", CH:" +  QString::number(chTo));
  for( int k = 0; k < (int) PHA::CH::AllSettings.size(); k ++){
    if( PHA::CH::AllSettings[k].ReadWrite() != RW::ReadWrite ) continue;
    if( !digi[digiTo]->WriteValue( PHA::CH::AllSettings[k],  digi[digiFrom]->GetSettingValue(PHA::CH::AllSettings[k], chFrom) , chTo ) ){
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

  SendLogMsg("Copy Settings from DIG:" +  QString::number(digi[digiFromIndex]->GetSerialNumber()) + " to DIG:" +  QString::number(digi[digiToIndex]->GetSerialNumber()));
  for( int i = 0; i < MaxNumberOfChannel; i++){
  if( chkChTo[i]->isChecked() ){
    //Copy setting
    for( int k = 0; k < (int) PHA::DIG::AllSettings.size(); k ++){
      if( PHA::DIG::AllSettings[k].ReadWrite() != RW::ReadWrite ) continue;
        if( ! digi[digiToIndex]->WriteValue( PHA::DIG::AllSettings[k],  digi[digiFromIndex]->GetSettingValue(PHA::DIG::AllSettings[k])) ){
          SendLogMsg("something wrong when copying setting : " + QString::fromStdString( PHA::DIG::AllSettings[k].GetPara())) ;
          return false;
          break;
        }
      }
    }
  }
  SendLogMsg("------ done");
  ID = digiToIndex;
  ShowSettingsToPanel();
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
