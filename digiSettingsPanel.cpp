#include "digiSettingsPanel.h"

#include <QLabel>
#include <QFileDialog>

std::vector<std::pair<std::string, int>> infoIndex = {{"Serial Num : ",            8},
                                                      {"IP : ",                    20},
                                                      {"Model Name : ",            5},
                                                      {"FPGA version : ",          1},
                                                      {"DPP Type : ",              2},
                                                      {"CUP version : ",           0},
                                                      {"ADC bits : ",              15},
                                                      {"ADC rate [Msps] : ",       16},
                                                      {"Num. of Channel : ",       14},
                                                      {"Input range [Vpp] : ",     17},
                                                      {"Input Type : ",            18},
                                                      {"Input Impedance [Ohm] : ", 19}
                                                    };

DigiSettingsPanel::DigiSettingsPanel(Digitizer2Gen ** digi, unsigned short nDigi, QWidget * parent) : QWidget(parent){

  qDebug() << "DigiSettingsPanel constructor";

  setWindowTitle("Digitizers Settings");
  setGeometry(0, 0, 1650, 1000);
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
      QGridLayout * infoLayout = new QGridLayout(infoBox);
      tabLayout_V1->addWidget(infoBox);
      
      const unsigned short nRow = 4;
      for( unsigned short j = 0; j < (unsigned short) infoIndex.size(); j++){
        QLabel * lab = new QLabel(QString::fromStdString(infoIndex[j].first), tab);
        lab->setAlignment(Qt::AlignRight);
        leInfo[iDigi][j] = new QLineEdit(tab);
        leInfo[iDigi][j]->setReadOnly(true);
        leInfo[iDigi][j]->setText(QString::fromStdString(digi[iDigi]->ReadValue(TYPE::DIG, infoIndex[j].second)));
        infoLayout->addWidget(lab, j%nRow, 2*(j/nRow));
        infoLayout->addWidget(leInfo[iDigi][j], j%nRow, 2*(j/nRow) +1);
      }
    }

    {//^====================== Group Board status
      QGroupBox * statusBox = new QGroupBox("Board Status", tab);
      //QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
      //sizePolicy.setHorizontalStretch(0);
      //sizePolicy.setVerticalStretch(0);
      //statusBox->setSizePolicy(sizePolicy);
      QGridLayout * statusLayout = new QGridLayout(statusBox);
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
        statusLayout->addWidget(LEDStatus[iDigi][i], 0, 1 + i);
      }

      //------- ACD Status
      QLabel * lbACQ = new QLabel("ACQ status : ");
      lbACQ->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      statusLayout->addWidget(lbACQ, 1, 0);

      for( int i = 0; i < 7; i++){
        ACQStatus[iDigi][i] = new QPushButton(tab);
        ACQStatus[iDigi][i]->setEnabled(false);
        ACQStatus[iDigi][i]->setFixedSize(QSize(30,30));
        statusLayout->addWidget(ACQStatus[iDigi][i], 1, 1 + i);
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
    }

    {//^====================== Board Setting Buttons
      QGridLayout * bnLayout = new QGridLayout();
      tabLayout_V2->addLayout(bnLayout);
      
      int rowId = 0;
      //-------------------------------------
      QLabel * lbSettingFile = new QLabel("Setting File : ", tab);
      lbSettingFile->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      bnLayout->addWidget(lbSettingFile, rowId, 0);

      leSettingFile[iDigi] = new QLineEdit(tab);
      leSettingFile[iDigi]->setReadOnly(true);
      bnLayout->addWidget(leSettingFile[iDigi], rowId, 1, 1, 9);
      
      //-------------------------------------
      rowId ++;
      QPushButton * bnReadSettngs = new QPushButton("Refresh Settings", tab);
      bnLayout->addWidget(bnReadSettngs, rowId, 0, 1, 2);
      connect(bnReadSettngs, &QPushButton::clicked, this, &DigiSettingsPanel::RefreshSettings);
      
      QPushButton * bnResetBd = new QPushButton("Reset Board", tab);
      bnLayout->addWidget(bnResetBd, rowId, 2, 1, 2);
      connect(bnResetBd, &QPushButton::clicked, this, [=](){
         emit sendLogMsg("Reset Digitizer-" + QString::number(digi[ID]->GetSerialNumber()));
         digi[ID]->Reset();    
      });
      
      QPushButton * bnDefaultSetting = new QPushButton("Set Default Settings", tab);
      bnLayout->addWidget(bnDefaultSetting, rowId, 4, 1, 2);
      connect(bnDefaultSetting, &QPushButton::clicked, this, [=](){
        emit sendLogMsg("Program Digitizer-" + QString::number(digi[ID]->GetSerialNumber()) + " to default PHA.");
        digi[ID]->ProgramPHA();
      });

      QPushButton * bnSaveSettings = new QPushButton("Save Settings", tab);
      bnLayout->addWidget(bnSaveSettings, rowId, 6, 1, 2);
      connect(bnSaveSettings, &QPushButton::clicked, this, &DigiSettingsPanel::SaveSettings);

      QPushButton * bnLoadSettings = new QPushButton("Load Settings", tab);
      bnLayout->addWidget(bnLoadSettings, rowId, 8, 1, 2);
      connect(bnLoadSettings, &QPushButton::clicked, this, &DigiSettingsPanel::LoadSettings);

      //---------------------------------------
      rowId ++;
      QPushButton * bnClearData = new QPushButton("Clear Data", tab);
      bnLayout->addWidget(bnClearData, rowId, 0, 1, 2);
      connect(bnClearData, &QPushButton::clicked, this, [=](){ 
          digi[ID]->SendCommand(DIGIPARA::DIG::ClearData); });
      
      QPushButton * bnArmACQ = new QPushButton("Arm ACQ", tab);
      bnLayout->addWidget(bnArmACQ, rowId, 2, 1, 2);
      connect(bnArmACQ, &QPushButton::clicked, this, [=](){ 
          digi[ID]->SendCommand(DIGIPARA::DIG::ArmACQ); });
      
      QPushButton * bnDisarmACQ = new QPushButton("Disarm ACQ", tab);
      bnLayout->addWidget(bnDisarmACQ, rowId, 4, 1, 2);
      connect(bnDisarmACQ, &QPushButton::clicked, this, [=](){ 
          digi[ID]->SendCommand(DIGIPARA::DIG::DisarmACQ); });

      QPushButton * bnSoftwareStart= new QPushButton("Software Start ACQ", tab);
      bnLayout->addWidget(bnSoftwareStart, rowId, 6, 1, 2);
      connect(bnSoftwareStart, &QPushButton::clicked, this, [=](){ 
          digi[ID]->SendCommand(DIGIPARA::DIG::SoftwareStartACQ); });

      QPushButton * bnSoftwareStop= new QPushButton("Software Stop ACQ", tab);
      bnLayout->addWidget(bnSoftwareStop, rowId, 8, 1, 2);
      connect(bnSoftwareStop, &QPushButton::clicked, this, [=](){ 
          digi[ID]->SendCommand(DIGIPARA::DIG::SoftwareStopACQ); });

    }

    {//^====================== Group Board settings
      QGroupBox * digiBox = new QGroupBox("Board Settings", tab);
      QGridLayout * boardLayout = new QGridLayout(digiBox);
      tabLayout_V2->addWidget(digiBox);
        
      int rowId = 0;
      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbClockSource[iDigi], DIGIPARA::DIG::ClockSource, tab, "Clock Source :", boardLayout, rowId, 0, 1, 2);

      //-------------------------------------
      rowId ++;
      QLabel * lbStartSource = new QLabel("Start Source :", tab);
      lbStartSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbStartSource, rowId, 0);

      for( int i = 0; i < (int) DIGIPARA::DIG::StartSource.GetAnswers().size(); i++){
        ckbStartSource[iDigi][i] = new QCheckBox( QString::fromStdString((DIGIPARA::DIG::StartSource.GetAnswers())[i].second), tab);
        boardLayout->addWidget(ckbStartSource[iDigi][i], rowId, 1 + i);
      }

      //-------------------------------------
      rowId ++;
      QLabel * lbGlobalTrgSource = new QLabel("Global Trigger Source :", tab);
      lbGlobalTrgSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbGlobalTrgSource, rowId, 0);

      for( int i = 0; i < (int) DIGIPARA::DIG::GlobalTriggerSource.GetAnswers().size(); i++){
        ckbGlbTrgSource[iDigi][i] = new QCheckBox( QString::fromStdString((DIGIPARA::DIG::GlobalTriggerSource.GetAnswers())[i].second), tab);
        boardLayout->addWidget(ckbGlbTrgSource[iDigi][i], rowId, 1 + i);
      }

      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbTrgOut[iDigi], DIGIPARA::DIG::TrgOutMode, tab, "Trg-OUT Mode :", boardLayout, rowId, 0, 1, 2);
            
      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbGPIO[iDigi], DIGIPARA::DIG::GPIOMode, tab, "GPIO Mode :", boardLayout, rowId, 0, 1, 2);

      //-------------------------------------
      QLabel * lbAutoDisarmAcq = new QLabel("Auto disarm ACQ :", tab);
      lbAutoDisarmAcq->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbAutoDisarmAcq, rowId, 4, 1, 2);

      cbbAutoDisarmAcq[iDigi] = new QComboBox(tab);
      boardLayout->addWidget(cbbAutoDisarmAcq[iDigi], rowId, 6);
      SetupCombBox(cbbAutoDisarmAcq[iDigi], DIGIPARA::DIG::EnableAutoDisarmACQ);

      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbBusyIn[iDigi], DIGIPARA::DIG::BusyInSource, tab, "Busy In Source :", boardLayout, rowId, 0, 1, 2);

      //-------------------------------------
      QLabel * lbStatEvents = new QLabel("Stat. Event :", tab);
      lbStatEvents->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbStatEvents, rowId, 4, 1, 2);

      cbbStatEvents[iDigi] = new QComboBox(tab);
      boardLayout->addWidget(cbbStatEvents[iDigi], rowId, 6);
      SetupCombBox(cbbStatEvents[iDigi], DIGIPARA::DIG::EnableStatisticEvents);

      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbSyncOut[iDigi], DIGIPARA::DIG::SyncOutMode, tab, "Sync Out mode :", boardLayout, rowId, 0, 1, 2);

      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbBoardVetoSource[iDigi], DIGIPARA::DIG::BoardVetoSource, tab, "Board Veto Source :", boardLayout, rowId, 0, 1, 2);

      QLabel * lbBdVetoWidth = new QLabel("Board Veto Width [ns] :", tab);
      lbBdVetoWidth->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbBdVetoWidth, rowId, 3, 1, 2);

      spbBdVetoWidth[iDigi] = new QSpinBox(tab); // may be QDoubleSpinBox
      spbBdVetoWidth[iDigi]->setMinimum(0);
      spbBdVetoWidth[iDigi]->setMaximum(38360);
      spbBdVetoWidth[iDigi]->setSingleStep(20);
      boardLayout->addWidget(spbBdVetoWidth[iDigi], rowId, 5);

      cbbBdVetoPolarity[iDigi] = new QComboBox(tab);
      boardLayout->addWidget(cbbBdVetoPolarity[iDigi], rowId, 6);
      SetupCombBox(cbbBdVetoPolarity[iDigi], DIGIPARA::DIG::BoardVetoPolarity);
      
      //-------------------------------------
      rowId ++;
      QLabel * lbRunDelay = new QLabel("Run Delay [ns] :", tab);
      lbRunDelay->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbRunDelay, rowId, 0);

      spbRunDelay[iDigi] = new QSpinBox(tab);
      spbRunDelay[iDigi]->setMinimum(0);
      spbRunDelay[iDigi]->setMaximum(524280);
      spbRunDelay[iDigi]->setSingleStep(20);
      boardLayout->addWidget(spbRunDelay[iDigi], rowId, 1);

      //-------------------------------------
      QLabel * lbClockOutDelay = new QLabel("Temp. Clock Out Delay [ps] :", tab);
      lbClockOutDelay->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbClockOutDelay, rowId, 3, 1, 2);

      dsbVolatileClockOutDelay[iDigi] = new QDoubleSpinBox(tab);
      dsbVolatileClockOutDelay[iDigi]->setMinimum(-18888.888);
      dsbVolatileClockOutDelay[iDigi]->setMaximum(18888.888);
      dsbVolatileClockOutDelay[iDigi]->setValue(0);
      boardLayout->addWidget(dsbVolatileClockOutDelay[iDigi], rowId, 5);

      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbIOLevel[iDigi], DIGIPARA::DIG::IO_Level, tab, "IO Level :", boardLayout, rowId, 0, 1, 2);


      QLabel * lbClockOutDelay2 = new QLabel("Perm. Clock Out Delay [ps] :", tab);
      lbClockOutDelay2->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbClockOutDelay2, rowId, 3, 1, 2);

      dsbClockOutDelay[iDigi] = new QDoubleSpinBox(tab);
      dsbClockOutDelay[iDigi]->setMinimum(-18888.888);
      dsbClockOutDelay[iDigi]->setMaximum(18888.888);
      dsbClockOutDelay[iDigi]->setValue(0);
      boardLayout->addWidget(dsbClockOutDelay[iDigi], rowId, 5);

      //-------------------------------------- Test pulse

    }

    {//^====================== Test Pulse settings
      testPulseBox = new QGroupBox("Test Pulse Settings", tab); 
      tabLayout_V2->addWidget(testPulseBox);
      QGridLayout * testPulseLayout = new QGridLayout(testPulseBox);

      QLabel * lbtestPuslePeriod = new QLabel("Period [ns] :", tab);
      lbtestPuslePeriod->setAlignment(Qt::AlignRight);
      testPulseLayout->addWidget(lbtestPuslePeriod, 0, 0);

      dsbTestPuslePeriod[iDigi] = new QDoubleSpinBox(tab);
      dsbTestPuslePeriod[iDigi]->setMinimum(0);
      dsbTestPuslePeriod[iDigi]->setMaximum(34359738360);
      testPulseLayout->addWidget(dsbTestPuslePeriod[iDigi], 0, 1);

      QLabel * lbtestPusleWidth = new QLabel("Width [ns] :", tab);
      lbtestPusleWidth->setAlignment(Qt::AlignRight);
      testPulseLayout->addWidget(lbtestPusleWidth, 0, 2);

      dsbTestPusleWidth[iDigi] = new QDoubleSpinBox(tab);
      dsbTestPusleWidth[iDigi]->setMinimum(0);
      dsbTestPusleWidth[iDigi]->setMaximum(34359738360);
      testPulseLayout->addWidget(dsbTestPusleWidth[iDigi], 0, 3);

      //------------------------------
      QLabel * lbtestPusleLow = new QLabel("Low Level [LSB] :", tab);
      lbtestPusleLow->setAlignment(Qt::AlignRight);
      testPulseLayout->addWidget(lbtestPusleLow, 1, 0);

      spbTestPusleLowLevel[iDigi] = new QSpinBox(tab);
      spbTestPusleLowLevel[iDigi]->setMinimum(0);
      spbTestPusleLowLevel[iDigi]->setMaximum(65535);
      testPulseLayout->addWidget(spbTestPusleLowLevel[iDigi], 1, 1);

      QLabel * lbtestPusleHigh = new QLabel("High Level [LSB] :", tab);
      lbtestPusleHigh->setAlignment(Qt::AlignRight);
      testPulseLayout->addWidget(lbtestPusleHigh, 1, 2);

      spbTestPusleHighLevel[iDigi] = new QSpinBox(tab);
      spbTestPusleHighLevel[iDigi]->setMinimum(0);
      spbTestPusleHighLevel[iDigi]->setMaximum(65535);
      testPulseLayout->addWidget(spbTestPusleHighLevel[iDigi], 1, 3);
    }


    {//^====================== Group channel settings
      QGroupBox * chBox = new QGroupBox("Channel Settings", tab); 
      tabLayout_V2->addWidget(chBox);
      QGridLayout * chLayout = new QGridLayout(chBox); //chBox->setLayout(chLayout);

      QTabWidget * chTabWidget = new QTabWidget(tab); chLayout->addWidget(chTabWidget);

      QSignalMapper * onOffMapper = new QSignalMapper(tab);
      connect(onOffMapper, &QSignalMapper::mappedInt, this, &DigiSettingsPanel::onChannelonOff); 
            
      {//.......... All Settings tab
        QWidget * tab_All = new QWidget(tab); 
        chTabWidget->addTab(tab_All, "All Settings");
        //tab_All->setStyleSheet("background-color: #EEEEEE");

        QGridLayout * allLayout = new QGridLayout(tab_All);
        //allLayout->setHorizontalSpacing(0);
        //allLayout->setVerticalSpacing(0);
        //QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        //sizePolicy.setHorizontalStretch(0);
        //sizePolicy.setVerticalStretch(0);
        //tab_All->setSizePolicy(sizePolicy);


        unsigned short ch = digi[iDigi]->GetNChannels();

        int rowID = 0;

        ckbChEnabled[iDigi][ch] = new QCheckBox("On/Off", tab); allLayout->addWidget(ckbChEnabled[iDigi][ch], rowID, 0);
        onOffMapper->setMapping(ckbChEnabled[iDigi][ch], (iDigi << 12) + ch);
        connect(ckbChEnabled[iDigi][ch], SIGNAL(clicked()), onOffMapper, SLOT(map()));

        SetupComboBox(cbbParity[iDigi][ch], DIGIPARA::CH::Polarity, tab, "Parity", allLayout, rowID, 0);

        QLabel * lbDCOffset = new QLabel("DC Offset [%]", tab); allLayout->addWidget(lbDCOffset, rowID, 3);
        lbDCOffset->setAlignment(Qt::AlignRight | Qt::AlignCenter);
        spbDCOffset[iDigi][ch] = new QSpinBox(tab); allLayout->addWidget(spbDCOffset[iDigi][ch], rowID, 4);

        QLabel * lbThreshold = new QLabel("Threshold [LSB]", tab); allLayout->addWidget(lbThreshold, rowID, 5);
        lbThreshold->setAlignment(Qt::AlignRight | Qt::AlignCenter);
        spbThreshold[iDigi][ch] = new QSpinBox(tab); allLayout->addWidget(spbThreshold[iDigi][ch], rowID, 6);


        //------------------------------
        rowID ++;
        SetupComboBox(cbbWaveSource[iDigi][ch], DIGIPARA::CH::WaveDataSource, tab, "Wave Data Source", allLayout, rowID, 0, 1, 2);
        //------------------------------
        rowID ++;
        SetupComboBox(cbbWaveRes[iDigi][ch], DIGIPARA::CH::WaveResolution, tab, "Wave Resol.", allLayout, rowID, 0);

        SetupComboBox(cbbWaveSave[iDigi][ch], DIGIPARA::CH::WaveSaving, tab, "Wave Save", allLayout, rowID, 2);
        //------------------------------
        rowID ++;
        QLabel * lbRL = new QLabel("Record Length [ns]", tab); allLayout->addWidget(lbRL, rowID, 0);
        lbRL->setAlignment(Qt::AlignRight| Qt::AlignCenter);
        spbRecordLength[iDigi][ch] = new QSpinBox(tab); allLayout->addWidget(spbRecordLength[iDigi][ch], rowID, 1);

        QLabel * lbPT = new QLabel("Pre Trigger [ns]", tab); allLayout->addWidget(lbPT, rowID, 2);
        lbPT->setAlignment(Qt::AlignRight| Qt::AlignCenter);
        spbPreTrigger[iDigi][ch] = new QSpinBox(tab); allLayout->addWidget(spbPreTrigger[iDigi][ch], rowID, 3);

        //------------------------------
        rowID ++;
        QLabel * lbInputRiseTime = new QLabel("Input Rise Time [ns]", tab); allLayout->addWidget(lbInputRiseTime, rowID, 0);
        lbInputRiseTime->setAlignment(Qt::AlignRight| Qt::AlignCenter);
        spbInputRiseTime[iDigi][ch] = new QSpinBox(tab); allLayout->addWidget(spbInputRiseTime[iDigi][ch], rowID, 1);

        QLabel * lbTriggerGuard = new QLabel("Trigger Guard [ns]", tab); allLayout->addWidget(lbTriggerGuard, rowID, 2);
        lbTriggerGuard->setAlignment(Qt::AlignRight| Qt::AlignCenter);
        spbTriggerGuard[iDigi][ch] = new QSpinBox(tab); allLayout->addWidget(spbTriggerGuard[iDigi][ch], rowID, 3);

        //------------------------------
        rowID ++;
        
        QLabel * lbTrapRiseTime = new QLabel("Trap. Rise Time [ns]", tab); allLayout->addWidget(lbTrapRiseTime, rowID, 0);
        lbTrapRiseTime->setAlignment(Qt::AlignRight| Qt::AlignCenter);
        spbTrapRiseTime[iDigi][ch] = new QSpinBox(tab); allLayout->addWidget(spbTrapRiseTime[iDigi][ch], rowID, 1);
        
        QLabel * lbTrapFlatTop = new QLabel("Trap. Flat Top [ns]", tab); allLayout->addWidget(lbTrapFlatTop, rowID, 2);
        lbTrapFlatTop->setAlignment(Qt::AlignRight| Qt::AlignCenter);
        spbTrapFlatTop[iDigi][ch] = new QSpinBox(tab); allLayout->addWidget(spbTrapFlatTop[iDigi][ch], rowID, 3);        

        QLabel * lbTrapPoleZero = new QLabel("Trap. Pole Zero [ns]", tab); allLayout->addWidget(lbTrapPoleZero, rowID, 4);
        lbTrapPoleZero->setAlignment(Qt::AlignRight| Qt::AlignCenter);
        spbTrapPoleZero[iDigi][ch] = new QSpinBox(tab); allLayout->addWidget(spbTrapPoleZero[iDigi][ch], rowID, 5);
        
        //------------------------------
        rowID ++;
        


      }

      
      {//.......... Ch On/Off
        QWidget * tab_onOff = new QWidget(tab); chTabWidget->addTab(tab_onOff, "On/Off");
        tab_onOff->setStyleSheet("background-color: #EEEEEE");

        QGridLayout * allLayout = new QGridLayout(tab_onOff); 
        allLayout->setHorizontalSpacing(0);
        allLayout->setVerticalSpacing(0);

        
        for( int ch = 0; ch < digi[iDigi]->GetNChannels(); ch++){
          ckbChEnabled[iDigi][ch] = new QCheckBox(QString::number(ch)); allLayout->addWidget(ckbChEnabled[iDigi][ch], ch/8, ch%8);
          ckbChEnabled[iDigi][ch]->setLayoutDirection(Qt::RightToLeft);

          onOffMapper->setMapping(ckbChEnabled[iDigi][ch], (iDigi << 12) + ch);
          connect(ckbChEnabled[iDigi][ch], SIGNAL(clicked()), onOffMapper, SLOT(map()));
        }

      }

    }

    {//^====================== Group trigger settings
      QGroupBox * triggerBox = new QGroupBox("Trigger Map", tab);
      QGridLayout * triggerLayout = new QGridLayout(triggerBox);
      //triggerBox->setLayout(triggerLayout);
      tabLayout_V1->addWidget(triggerBox);
      
      triggerLayout->setHorizontalSpacing(0);
      triggerLayout->setVerticalSpacing(0);

      QLabel * instr = new QLabel("Reading: Column (C) represents a trigger channel for Row (R) channel.\nFor example, R3C1 = ch-3 trigger source is ch-1.\n", tab);
      triggerLayout->addWidget(instr, 0, 0, 1, 64+15);

      QSignalMapper * triggerMapper = new QSignalMapper(tab);
      connect(triggerMapper, &QSignalMapper::mappedInt, this, &DigiSettingsPanel::onTriggerClick);

      int rowID = 1;
      int colID = 0;
      for(int i = 0; i < digi[iDigi]->GetNChannels(); i++){
        colID = 0;
        for(int j = 0; j < digi[iDigi]->GetNChannels(); j++){
          
          bn[i][j] = new QPushButton(tab);
          bn[i][j]->setFixedSize(QSize(10,10));
          bnClickStatus[i][j] = false;

          if( i%4 != 0 && j == (i/4)*4) {
            bn[i][j]->setStyleSheet("background-color: red;");
            bnClickStatus[i][j] = true;
          }
          triggerLayout->addWidget(bn[i][j], rowID, colID);

          triggerMapper->setMapping(bn[i][j], (iDigi << 12) + (i << 8) + j);
          connect(bn[i][j], SIGNAL(clicked()), triggerMapper, SLOT(map()));

          colID ++;

          if( j%4 == 3 && j!= digi[iDigi]->GetNChannels() - 1){
            QFrame * vSeparator = new QFrame(tab);
            vSeparator->setFrameShape(QFrame::VLine);
            triggerLayout->addWidget(vSeparator, rowID, colID);
            colID++;
          }
        }

        rowID++;

        if( i%4 == 3 && i != digi[iDigi]->GetNChannels() - 1){
          QFrame * hSeparator = new QFrame(tab);
          hSeparator->setFrameShape(QFrame::HLine);
          triggerLayout->addWidget(hSeparator, rowID, 0, 1, digi[iDigi]->GetNChannels() + 15);
          rowID++;
        }


      }
    }


  } //=== end of tab


}

DigiSettingsPanel::~DigiSettingsPanel(){

  printf("%s\n", __func__);

  for( int iDig = 0; iDig < nDigi; iDig ++){

/*
    for( int i = 0; i < 12; i++) delete leInfo[iDig][i];
    for( int i = 0; i < 19; i++) {
      delete LEDStatus[iDig][i];
      delete ACQStatus[iDig][i];
    }

    for( int i = 0; i < 8; i++) delete leTemp[iDig][i];
    
    delete cbbClockSource[iDig];

    for( int i = 0; i < 5; i++) {
      delete ckbStartSource[iDig][i];
      delete ckbGlbTrgSource[iDig][i];
    }
*/
    for( int i =0 ; i < MaxNumberOfChannel; i++){
      if( ckbChEnabled[iDig][i] != NULL) delete ckbChEnabled[iDig][i];
    }
  }

}

//^================================================================
void DigiSettingsPanel::onTriggerClick(int haha){
  
  unsigned short iDig = haha >> 12;
  unsigned short ch = (haha >> 8 ) & 0xF;
  unsigned short ch2 = haha & 0xFF;

  qDebug() << "Digi-" << iDig << ", Ch-" << ch << ", " << ch2; 

  if(bnClickStatus[ch][ch2]){
    bn[ch][ch2]->setStyleSheet(""); 
    bnClickStatus[ch][ch2] = false;
  }else{
    bn[ch][ch2]->setStyleSheet("background-color: red;"); 
    bnClickStatus[ch][ch2] = true;
  }
}

void DigiSettingsPanel::onChannelonOff(int haha){
  
  unsigned short iDig = haha >> 12;
  qDebug()<< "nDigi-" << iDig << ", ch-" << (haha & 0xFF);
  if( (haha & 0xFF) == 64){

    if( ckbChEnabled[iDig][64]->isChecked() ){
      for( int i = 0 ; i < digi[iDig]->GetNChannels() ; i++){
        ckbChEnabled[iDig][i]->setChecked(true);
      }
    }else{
      for( int i = 0 ; i < digi[iDig]->GetNChannels() ; i++){
        ckbChEnabled[iDig][i]->setChecked(false);
      }
    }
  }else{
    unsigned int nOn = 0;
    for( int i = 0; i < digi[iDig]->GetNChannels(); i++){
      nOn += (ckbChEnabled[iDig][i]->isChecked() ? 1 : 0);
    }

    if( nOn == 64){
      ckbChEnabled[iDig][64]->setChecked(true);
    }else{
      ckbChEnabled[iDig][64]->setChecked(false);
    }

  }
}

//^================================================================

void DigiSettingsPanel::RefreshSettings(){
  digi[ID]->ReadAllSettings();
  ShowSettingsToPanel();
}

void DigiSettingsPanel::SaveSettings(){



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
    emit sendLogMsg("Loaded settings file " + fileName + " for Digi-" + QString::number(digi[ID]->GetSerialNumber()));
  }else{
    emit sendLogMsg("Fail to Loaded settings file " + fileName + " for Digi-" + QString::number(digi[ID]->GetSerialNumber()));
  }

  //TODO ==== show result
  ShowSettingsToPanel();
}

void DigiSettingsPanel::ShowSettingsToPanel(){

  enableSignalSlot = false;

  for (unsigned short j = 0; j < (unsigned short) infoIndex.size(); j++){
    leInfo[ID][j]->setText(QString::fromStdString(digi[ID]->GetSettingValue(TYPE::DIG, infoIndex[j].second)));
  } 

  //--------- LED Status
  unsigned int ledStatus = atoi(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::LED_status).c_str());
  for( int i = 0; i < 19; i++){
    if( (ledStatus >> i) & 0x1 ) {
      LEDStatus[ID][i]->setStyleSheet("background-color:green;");
    }else{
      LEDStatus[ID][i]->setStyleSheet("");
    }
  }

  //--------- ACQ Status
  unsigned int acqStatus = atoi(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::ACQ_status).c_str());
  for( int i = 0; i < 7; i++){
    if( (acqStatus >> i) & 0x1 ) {
      ACQStatus[ID][i]->setStyleSheet("background-color:green;");
    }else{
      ACQStatus[ID][i]->setStyleSheet("");
    }
  }

  //-------- temperature
  for( int i = 0; i < 8; i++){
    leTemp[ID][i]->setText(QString::fromStdString(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::TempSensADC[i])));
  }

  //-------- board settings
  ReadCombBoxValue(cbbClockSource[ID], TYPE::DIG, DIGIPARA::DIG::ClockSource);

  QString result = QString::fromStdString(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::StartSource));
  QStringList resultList = result.remove(QChar(' ')).split("|");
  //qDebug() << resultList << "," << resultList.count();
  for( int j = 0; j < (int) DIGIPARA::DIG::StartSource.GetAnswers().size(); j++){
    ckbStartSource[ID][j]->setChecked(false);
    for( int i = 0; i < resultList.count(); i++){
      //qDebug() << resultList[i] << ", " << QString::fromStdString((DIGIPARA::DIG::StartSource.GetAnswers())[j].first);
      if( resultList[i] == QString::fromStdString((DIGIPARA::DIG::StartSource.GetAnswers())[j].first) ) ckbStartSource[ID][j]->setChecked(true);
    }
  }


  result = QString::fromStdString(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::GlobalTriggerSource));
  resultList = result.remove(QChar(' ')).split("|");
  for( int j = 0; j < (int) DIGIPARA::DIG::StartSource.GetAnswers().size(); j++){
    ckbGlbTrgSource[ID][j]->setChecked(false);
    for( int i = 0; i < resultList.count(); i++){
      if( resultList[i] == QString::fromStdString((DIGIPARA::DIG::GlobalTriggerSource.GetAnswers())[j].first) ) ckbGlbTrgSource[ID][j]->setChecked(true);
    }
  }

  ReadCombBoxValue(cbbTrgOut[ID], TYPE::DIG, DIGIPARA::DIG::TrgOutMode);
  ReadCombBoxValue(cbbGPIO[ID], TYPE::DIG, DIGIPARA::DIG::GPIOMode);
  ReadCombBoxValue(cbbBusyIn[ID], TYPE::DIG, DIGIPARA::DIG::BusyInSource);
  ReadCombBoxValue(cbbSyncOut[ID], TYPE::DIG, DIGIPARA::DIG::SyncOutMode);
  ReadCombBoxValue(cbbAutoDisarmAcq[ID], TYPE::DIG, DIGIPARA::DIG::EnableAutoDisarmACQ);
  ReadCombBoxValue(cbbStatEvents[ID], TYPE::DIG, DIGIPARA::DIG::EnableStatisticEvents);
  ReadCombBoxValue(cbbBdVetoPolarity[ID], TYPE::DIG, DIGIPARA::DIG::BoardVetoPolarity);
  ReadCombBoxValue(cbbBoardVetoSource[ID], TYPE::DIG, DIGIPARA::DIG::BoardVetoSource);
  ReadCombBoxValue(cbbIOLevel[ID], TYPE::DIG, DIGIPARA::DIG::IO_Level);

  result = QString::fromStdString(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::BoardVetoWidth));
  spbBdVetoWidth[ID]->setValue(result.toInt());
  
  result = QString::fromStdString(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::RunDelay));
  spbRunDelay[ID]->setValue(result.toInt());
  
  result = QString::fromStdString(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::VolatileClockOutDelay));
  dsbVolatileClockOutDelay[ID]->setValue(result.toDouble());
  
  result = QString::fromStdString(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::PermanentClockOutDelay));
  dsbClockOutDelay[ID]->setValue(result.toDouble());

  //------------- test pulse
  result = QString::fromStdString(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::TestPulsePeriod));
  dsbTestPuslePeriod[ID]->setValue(result.toDouble());
  result = QString::fromStdString(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::TestPulseWidth));
  dsbTestPusleWidth[ID]->setValue(result.toDouble());
  result = QString::fromStdString(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::TestPulseLowLevel));
  spbTestPusleLowLevel[ID]->setValue(result.toInt());
  result = QString::fromStdString(digi[ID]->GetSettingValue(TYPE::DIG, DIGIPARA::DIG::TestPulseHighLevel));
  spbTestPusleHighLevel[ID]->setValue(result.toInt());


  
  
  enableSignalSlot = true;

}

void DigiSettingsPanel::SetupCombBox(QComboBox *cbb, Reg para){
  for( int i = 0 ; i < (int) para.GetAnswers().size(); i++){
    cbb->addItem(QString::fromStdString((para.GetAnswers())[i].second), 
                QString::fromStdString((para.GetAnswers())[i].first));
  }
}

void DigiSettingsPanel::SetupComboBox(QComboBox *cbb, Reg para, QWidget * parent, QString labelTxt, QGridLayout *layout, int row, int col, int srow, int scol){
  QLabel * lb = new QLabel(labelTxt, parent); 
  layout->addWidget(lb, row, col);
  lb->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  cbb = new QComboBox(parent); 
  layout->addWidget(cbb, row, col+1, srow, scol);
  for( int i = 0 ; i < (int) para.GetAnswers().size(); i++){
    cbb->addItem(QString::fromStdString((para.GetAnswers())[i].second), 
                QString::fromStdString((para.GetAnswers())[i].first));
  }

}

void DigiSettingsPanel::ReadCombBoxValue(QComboBox *cbb, TYPE type, Reg para)
{
  QString result = QString::fromStdString(digi[ID]->GetSettingValue(type, para));
  int index = cbb->findData(result);
  if( index >= 0 && index < cbb->count()) {
    cbb->setCurrentIndex(index);
  }else{
    qDebug() << result;
  }
}
