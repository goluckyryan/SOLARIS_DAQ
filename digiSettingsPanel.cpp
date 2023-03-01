#include "digiSettingsPanel.h"

#include <QLabel>
#include <QFileDialog>

std::vector<std::pair<std::string, Reg>> infoIndex = {{"Serial Num : ",            DIGIPARA::DIG::SerialNumber},
                                                      {"IP : ",                    DIGIPARA::DIG::IPAddress},
                                                      {"Model Name : ",            DIGIPARA::DIG::ModelName},
                                                      {"FPGA version : ",          DIGIPARA::DIG::FPGA_firmwareVersion},
                                                      {"DPP Type : ",              DIGIPARA::DIG::FirmwareType},
                                                      {"CUP version : ",           DIGIPARA::DIG::CupVer},
                                                      {"ADC bits : ",              DIGIPARA::DIG::ADC_bit},
                                                      {"ADC rate [Msps] : ",       DIGIPARA::DIG::ADC_SampleRate},
                                                      {"Num. of Channel : ",       DIGIPARA::DIG::NumberOfChannel},
                                                      {"Input range [Vpp] : ",     DIGIPARA::DIG::InputDynamicRange},
                                                      {"Input Type : ",            DIGIPARA::DIG::InputType},
                                                      {"Input Impedance [Ohm] : ", DIGIPARA::DIG::InputImpedance}
                                                    };

DigiSettingsPanel::DigiSettingsPanel(Digitizer2Gen ** digi, unsigned short nDigi, QWidget * parent) : QWidget(parent){

  qDebug() << "DigiSettingsPanel constructor";

  setWindowTitle("Digitizers Settings");
  setGeometry(0, 0, 1800, 900);
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


      //--------------- 
      if( digi[iDigi]->IsDummy() ){
        bnReadSettngs->setEnabled(false);
        bnResetBd->setEnabled(false);
        bnDefaultSetting->setEnabled(false);
        bnClearData->setEnabled(false);
        bnArmACQ->setEnabled(false);
        bnDisarmACQ->setEnabled(false);
        bnSoftwareStart->setEnabled(false);
        bnSoftwareStop->setEnabled(false);
      }

    }

    
    {//^====================== Group Board settings
      QGroupBox * digiBox = new QGroupBox("Board Settings", tab);
      //digiBox->setSizePolicy(sizePolicy);
      QGridLayout * boardLayout = new QGridLayout(digiBox);
      tabLayout_V1->addWidget(digiBox);
        
      int rowId = 0;
      //-------------------------------------
      SetupComboBox(cbbClockSource[iDigi], DIGIPARA::DIG::ClockSource, -1, true, "Clock Source :", boardLayout, rowId, 0, 1, 2);

      //-------------------------------------
      rowId ++;
      QLabel * lbStartSource = new QLabel("Start Source :", tab);
      lbStartSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbStartSource, rowId, 0);

      for( int i = 0; i < (int) DIGIPARA::DIG::StartSource.GetAnswers().size(); i++){
        ckbStartSource[iDigi][i] = new QCheckBox( QString::fromStdString((DIGIPARA::DIG::StartSource.GetAnswers())[i].second), tab);
        boardLayout->addWidget(ckbStartSource[iDigi][i], rowId, 1 + i);
        connect(ckbStartSource[iDigi][i], &QCheckBox::stateChanged, this, &DigiSettingsPanel::SetStartSource);
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
      SetupComboBox(cbbTrgOut[iDigi], DIGIPARA::DIG::TrgOutMode, -1, true, "Trg-OUT Mode :", boardLayout, rowId, 0, 1, 2);
            
      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbGPIO[iDigi], DIGIPARA::DIG::GPIOMode, -1, true, "GPIO Mode :", boardLayout, rowId, 0, 1, 2);

      //-------------------------------------
      QLabel * lbAutoDisarmAcq = new QLabel("Auto disarm ACQ :", tab);
      lbAutoDisarmAcq->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbAutoDisarmAcq, rowId, 4, 1, 2);

      cbbAutoDisarmAcq[iDigi] = new QComboBox(tab);
      boardLayout->addWidget(cbbAutoDisarmAcq[iDigi], rowId, 6);
      SetupShortComboBox(cbbAutoDisarmAcq[iDigi], DIGIPARA::DIG::EnableAutoDisarmACQ);

      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbBusyIn[iDigi], DIGIPARA::DIG::BusyInSource, -1, true, "Busy In Source :", boardLayout, rowId, 0, 1, 2);

      QLabel * lbStatEvents = new QLabel("Stat. Event :", tab);
      lbStatEvents->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbStatEvents, rowId, 4, 1, 2);

      cbbStatEvents[iDigi] = new QComboBox(tab);
      boardLayout->addWidget(cbbStatEvents[iDigi], rowId, 6);
      SetupShortComboBox(cbbStatEvents[iDigi], DIGIPARA::DIG::EnableStatisticEvents);
      connect(cbbStatEvents[iDigi], &QComboBox::currentIndexChanged, this, [=](){
        if( !enableSignalSlot ) return;
        digi[ID]->WriteValue(DIGIPARA::DIG::EnableStatisticEvents, cbbStatEvents[ID]->currentData().toString().toStdString());
      });

      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbSyncOut[iDigi], DIGIPARA::DIG::SyncOutMode, -1, true, "Sync Out mode :", boardLayout, rowId, 0, 1, 2);

      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbBoardVetoSource[iDigi], DIGIPARA::DIG::BoardVetoSource, -1, true, "Board Veto Source :", boardLayout, rowId, 0, 1, 2);

      QLabel * lbBdVetoWidth = new QLabel("Board Veto Width [ns] :", tab);
      lbBdVetoWidth->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbBdVetoWidth, rowId, 3, 1, 2);

      spbBdVetoWidth[iDigi] = new QSpinBox(tab); // may be QDoubleSpinBox
      spbBdVetoWidth[iDigi]->setMinimum(0);
      spbBdVetoWidth[iDigi]->setMaximum(38360);
      spbBdVetoWidth[iDigi]->setSingleStep(20);
      boardLayout->addWidget(spbBdVetoWidth[iDigi], rowId, 5);
      connect(spbBdVetoWidth[iDigi], &QSpinBox::valueChanged, this, [=](){
        if( !enableSignalSlot ) return;
        //printf("%s %d  %d \n", para.GetPara().c_str(), ch_index, spb->value());
        digi[ID]->WriteValue(DIGIPARA::DIG::BoardVetoWidth, std::to_string(spbBdVetoWidth[iDigi]->value()), -1);
      });

      cbbBdVetoPolarity[iDigi] = new QComboBox(tab);
      boardLayout->addWidget(cbbBdVetoPolarity[iDigi], rowId, 6);
      SetupShortComboBox(cbbBdVetoPolarity[iDigi], DIGIPARA::DIG::BoardVetoPolarity);
      
      //-------------------------------------
      rowId ++;
      SetupSpinBox(spbRunDelay[iDigi], DIGIPARA::DIG::RunDelay, -1, "Run Delay [ns] :", boardLayout, rowId, 0);

      //-------------------------------------
      QLabel * lbClockOutDelay = new QLabel("Temp. Clock Out Delay [ps] :", tab);
      lbClockOutDelay->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbClockOutDelay, rowId, 3, 1, 2);

      dsbVolatileClockOutDelay[iDigi] = new QDoubleSpinBox(tab);
      dsbVolatileClockOutDelay[iDigi]->setMinimum(-18888.888);
      dsbVolatileClockOutDelay[iDigi]->setMaximum(18888.888);
      dsbVolatileClockOutDelay[iDigi]->setValue(0);
      boardLayout->addWidget(dsbVolatileClockOutDelay[iDigi], rowId, 5);
      connect(dsbVolatileClockOutDelay[iDigi], &QDoubleSpinBox::valueChanged, this, [=](){
        if( !enableSignalSlot ) return;
        //printf("%s %d  %d \n", para.GetPara().c_str(), ch_index, spb->value());
        digi[ID]->WriteValue(DIGIPARA::DIG::VolatileClockOutDelay, std::to_string(dsbVolatileClockOutDelay[iDigi]->value()), -1);
      });

      //-------------------------------------
      rowId ++;
      SetupComboBox(cbbIOLevel[iDigi], DIGIPARA::DIG::IO_Level, -1, true, "IO Level :", boardLayout, rowId, 0, 1, 2);

      QLabel * lbClockOutDelay2 = new QLabel("Perm. Clock Out Delay [ps] :", tab);
      lbClockOutDelay2->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbClockOutDelay2, rowId, 3, 1, 2);

      dsbClockOutDelay[iDigi] = new QDoubleSpinBox(tab);
      dsbClockOutDelay[iDigi]->setMinimum(-18888.888);
      dsbClockOutDelay[iDigi]->setMaximum(18888.888);
      dsbClockOutDelay[iDigi]->setValue(0);
      boardLayout->addWidget(dsbClockOutDelay[iDigi], rowId, 5);
      connect(dsbClockOutDelay[iDigi], &QDoubleSpinBox::valueChanged, this, [=](){
        if( !enableSignalSlot ) return;
        //printf("%s %d  %d \n", para.GetPara().c_str(), ch_index, spb->value());
        digi[ID]->WriteValue(DIGIPARA::DIG::VolatileClockOutDelay, std::to_string(dsbClockOutDelay[iDigi]->value()), -1);
      });
    }
    
    {//^====================== Test Pulse settings
      testPulseBox = new QGroupBox("Test Pulse Settings", tab); 
      //testPulseBox->setSizePolicy(sizePolicy);
      tabLayout_V1->addWidget(testPulseBox);
      QGridLayout * testPulseLayout = new QGridLayout(testPulseBox);
      testPulseLayout->setAlignment(Qt::AlignLeft);

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
      //chBox->setSizePolicy(sizePolicy);
      tabLayout_V2->addWidget(chBox);
      QGridLayout * chLayout = new QGridLayout(chBox); //chBox->setLayout(chLayout);

      QTabWidget * chTabWidget = new QTabWidget(tab); chLayout->addWidget(chTabWidget);

      {//@.......... All Settings tab
        QWidget * tab_All = new QWidget(tab); 
        //tab_All->setStyleSheet("background-color: #EEEEEE");
        chTabWidget->addTab(tab_All, "All Settings");

        QGridLayout * allLayout = new QGridLayout(tab_All);
        allLayout->setAlignment(Qt::AlignTop);

        unsigned short ch = digi[iDigi]->GetNChannels();

        int rowID = 0;
        {//*--------- Group 1
          QGroupBox * box1 = new QGroupBox("Input Settings", tab);
          allLayout->addWidget(box1);
          QGridLayout * layout1 = new QGridLayout(box1);

          rowID = 0;
          SetupComboBox(cbbOnOff[iDigi][ch], DIGIPARA::CH::ChannelEnable, -1, true, "On/Off", layout1, rowID, 0);

          rowID ++;
          SetupComboBox(cbbWaveSource[iDigi][ch], DIGIPARA::CH::WaveDataSource, -1, true, "Wave Data Source", layout1, rowID, 0, 1, 2);

          rowID ++;
          SetupComboBox(cbbWaveRes[iDigi][ch], DIGIPARA::CH::WaveResolution, -1, true,  "Wave Resol.", layout1, rowID, 0);
          SetupComboBox(cbbWaveSave[iDigi][ch], DIGIPARA::CH::WaveSaving, -1, true, "Wave Save", layout1, rowID, 2);

          rowID ++;
          SetupComboBox(cbbParity[iDigi][ch], DIGIPARA::CH::Polarity, -1, true, "Parity", layout1, rowID, 0);
          SetupComboBox(cbbLowFilter[iDigi][ch], DIGIPARA::CH::EnergyFilterLowFreqFilter, -1, true, "Low Freq. Filter", layout1, rowID, 2);

          rowID ++;
          SetupSpinBox(spbDCOffset[iDigi][ch], DIGIPARA::CH::DC_Offset, -1, "DC Offset [%]", layout1, rowID, 0);
          SetupSpinBox(spbThreshold[iDigi][ch], DIGIPARA::CH::TriggerThreshold, -1, "Threshold [LSB]", layout1, rowID, 2);

          rowID ++;
          SetupSpinBox(spbInputRiseTime[iDigi][ch], DIGIPARA::CH::TimeFilterRiseTime, -1, "Input Rise Time [ns]", layout1, rowID, 0);
          SetupSpinBox(spbTriggerGuard[iDigi][ch], DIGIPARA::CH::TimeFilterRetriggerGuard, -1, "Trigger Guard [ns]", layout1, rowID, 2);

          rowID ++;
          SetupSpinBox(spbRecordLength[iDigi][ch], DIGIPARA::CH::RecordLength, -1, "Record Length [ns]", layout1, rowID, 0);
          SetupSpinBox(spbPreTrigger[iDigi][ch], DIGIPARA::CH::PreTrigger, -1, "Pre Trigger [ns]", layout1, rowID, 2);

        }

        {//*--------- Group 3
          QGroupBox * box3 = new QGroupBox("Trap. Settings", tab);
          allLayout->addWidget(box3);
          QGridLayout * layout3 = new QGridLayout(box3);

          //------------------------------
          rowID = 0;    
          SetupSpinBox(spbTrapRiseTime[iDigi][ch], DIGIPARA::CH::EnergyFilterRiseTime, -1, "Trap. Rise Time [ns]", layout3, rowID, 0);
          SetupSpinBox(spbTrapFlatTop[iDigi][ch], DIGIPARA::CH::EnergyFilterFlatTop, -1, "Trap. Flat Top [ns]", layout3, rowID, 2);
          SetupSpinBox(spbTrapPoleZero[iDigi][ch], DIGIPARA::CH::EnergyFilterPoleZero, -1, "Trap. Pole Zero [ns]", layout3, rowID, 4);
          
          //------------------------------
          rowID ++;
          SetupSpinBox(spbPeaking[iDigi][ch], DIGIPARA::CH::EnergyFilterPeakingPosition, -1, "Peaking [%]", layout3, rowID, 0);
          SetupSpinBox(spbBaselineGuard[iDigi][ch], DIGIPARA::CH::EnergyFilterBaselineGuard, -1, "Baseline Guard [ns]", layout3, rowID, 2);
          SetupSpinBox(spbPileupGuard[iDigi][ch], DIGIPARA::CH::EnergyFilterPileUpGuard, -1, "Pile-up Guard [ns]", layout3, rowID, 4);
          
          //------------------------------
          rowID ++;
          SetupComboBox(cbbPeakingAvg[iDigi][ch], DIGIPARA::CH::EnergyFilterPeakingAvg, -1, true, "Peak Avg", layout3, rowID, 0);
          SetupComboBox(cbbBaselineAvg[iDigi][ch], DIGIPARA::CH::EnergyFilterBaselineAvg, -1, true, "Baseline Avg", layout3, rowID, 2);
          SetupSpinBox(spbFineGain[iDigi][ch], DIGIPARA::CH::EnergyFilterFineGain, -1, "Fine Gain", layout3, rowID, 4);
          
        }

        {//*--------- Group 4
          QGroupBox * box4 = new QGroupBox("Probe Settings", tab);
          allLayout->addWidget(box4);
          QGridLayout * layout4 = new QGridLayout(box4);

          //------------------------------
          rowID = 0;
          SetupComboBox(cbbAnaProbe0[iDigi][ch], DIGIPARA::CH::WaveAnalogProbe0, -1, true, "Analog Prob. 0", layout4, rowID, 0, 1, 2);
          SetupComboBox(cbbAnaProbe1[iDigi][ch], DIGIPARA::CH::WaveAnalogProbe1, -1, true, "Analog Prob. 1", layout4, rowID, 3, 1, 2);        

          //------------------------------
          rowID ++;
          SetupComboBox(cbbDigProbe0[iDigi][ch], DIGIPARA::CH::WaveDigitalProbe0, -1, true, "Digitial Prob. 0", layout4, rowID, 0, 1, 2);
          SetupComboBox(cbbDigProbe1[iDigi][ch], DIGIPARA::CH::WaveDigitalProbe1, -1, true, "Digitial Prob. 1", layout4, rowID, 3, 1, 2);

          //------------------------------
          rowID ++;
          SetupComboBox(cbbDigProbe2[iDigi][ch], DIGIPARA::CH::WaveDigitalProbe2, -1, true, "Digitial Prob. 2", layout4, rowID, 0, 1, 2);
          SetupComboBox(cbbDigProbe3[iDigi][ch], DIGIPARA::CH::WaveDigitalProbe3, -1, true, "Digitial Prob. 3", layout4, rowID, 3, 1, 2);
          
        }

        {//*--------- Group 5
          QGroupBox * box5 = new QGroupBox("Trigger Settings", tab);
          allLayout->addWidget(box5);
          QGridLayout * layout5 = new QGridLayout(box5);

          //------------------------------
          rowID = 0;
          SetupComboBox(cbbEvtTrigger[iDigi][ch], DIGIPARA::CH::EventTriggerSource, -1, true, "Event Trig. Source", layout5, rowID, 0);
          SetupComboBox(cbbWaveTrigger[iDigi][ch], DIGIPARA::CH::WaveTriggerSource, -1, true, "Wave Trig. Source", layout5, rowID, 2);

          //------------------------------
          rowID ++;
          SetupComboBox(cbbChVetoSrc[iDigi][ch], DIGIPARA::CH::ChannelVetoSource, -1, true, "Veto Source", layout5, rowID, 0);

          QLabel * lbTriggerMask = new QLabel("Trigger Mask", tab);
          lbTriggerMask->setAlignment(Qt::AlignCenter | Qt::AlignRight);
          layout5->addWidget(lbTriggerMask, rowID, 2);

          leTriggerMask[iDigi][ch] = new QLineEdit(tab);
          layout5->addWidget(leTriggerMask[iDigi][ch], rowID, 3);

          //------------------------------
          rowID ++;
          SetupComboBox(cbbCoinMask[iDigi][ch], DIGIPARA::CH::CoincidenceMask, -1, true, "Coin. Mask", layout5, rowID, 0);
          SetupComboBox(cbbAntiCoinMask[iDigi][ch], DIGIPARA::CH::AntiCoincidenceMask, -1, true, "Anti-Coin. Mask", layout5, rowID, 2);

          //------------------------------
          rowID ++;
          SetupSpinBox(spbCoinLength[iDigi][ch], DIGIPARA::CH::CoincidenceLength, -1, "Coin. Length [ns]", layout5, rowID, 0);
          SetupSpinBox(spbADCVetoWidth[iDigi][ch], DIGIPARA::CH::ADCVetoWidth, -1, "ADC Veto Length [ns]", layout5, rowID, 2);

          for( int i = 0; i < layout5->columnCount(); i++) layout5->setColumnStretch(i, 1);

        }

        {//*--------- Group 6
          QGroupBox * box6 = new QGroupBox("Other Settings", tab);
          allLayout->addWidget(box6);
          QGridLayout * layout6 = new QGridLayout(box6);

          //------------------------------
          rowID = 0 ;
          SetupComboBox(cbbEventSelector[iDigi][ch], DIGIPARA::CH::EventSelector, -1, true, "Event Selector", layout6, rowID, 0);
          SetupComboBox(cbbWaveSelector[iDigi][ch], DIGIPARA::CH::WaveSelector, -1, true, "Wave Selector", layout6, rowID, 2);

          //------------------------------
          rowID ++;
          SetupSpinBox(spbEnergySkimLow[iDigi][ch], DIGIPARA::CH::EnergySkimLowDiscriminator, -1, "Energy Skim Low", layout6, rowID, 0);
          SetupSpinBox(spbEnergySkimHigh[iDigi][ch], DIGIPARA::CH::EnergySkimHighDiscriminator, -1, "Energy Skim High", layout6, rowID, 2);
        }
      }

      {//@============== Status  tab
        QTabWidget * statusTab = new QTabWidget(tab);
        chTabWidget->addTab(statusTab, "Status");


      }
      
      
      {//@============== input  tab
        QTabWidget * inputTab = new QTabWidget(tab);
        chTabWidget->addTab(inputTab, "Input");

        SetupComboBoxTab(cbbOnOff, DIGIPARA::CH::ChannelEnable, "On/Off", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbDCOffset, DIGIPARA::CH::DC_Offset, "DC Offset [%]", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbThreshold, DIGIPARA::CH::TriggerThreshold, "Threshold [LSB]", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbParity, DIGIPARA::CH::Polarity, "Parity", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbRecordLength, DIGIPARA::CH::RecordLength, "Record Length [ns]", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbPreTrigger, DIGIPARA::CH::PreTrigger, "PreTrigger [ns]", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbInputRiseTime, DIGIPARA::CH::TimeFilterRiseTime, "Input Rise Time [ns]", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbTriggerGuard, DIGIPARA::CH::TimeFilterRetriggerGuard, "Trigger Guard [ns]", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbLowFilter, DIGIPARA::CH::EnergyFilterLowFreqFilter, "Low Freq. Filter", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbWaveSource, DIGIPARA::CH::WaveDataSource, "Wave Data Dource", inputTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbWaveRes, DIGIPARA::CH::WaveResolution,  "Wave Resol.", inputTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbWaveSave, DIGIPARA::CH::WaveSaving, "Wave Save", inputTab, iDigi, digi[iDigi]->GetNChannels());
      }
      
      {//@============== Trap  tab
        QTabWidget * trapTab = new QTabWidget(tab);
        chTabWidget->addTab(trapTab, "Trapezoid");

        SetupSpinBoxTab(spbTrapRiseTime, DIGIPARA::CH::EnergyFilterRiseTime, "Trap. Rise Time [ns]", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbTrapFlatTop, DIGIPARA::CH::EnergyFilterFlatTop, "Trap. Flat Top [ns]", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbTrapPoleZero, DIGIPARA::CH::EnergyFilterPoleZero, "Trap. Pole Zero [ns]", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbPeaking, DIGIPARA::CH::EnergyFilterPeakingAvg, "Peaking [%]", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbPeakingAvg, DIGIPARA::CH::EnergyFilterPeakingAvg, "Peak Avg.", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbBaselineAvg, DIGIPARA::CH::EnergyFilterBaselineAvg, "Baseline Avg.", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbFineGain, DIGIPARA::CH::EnergyFilterFineGain, "Fine Gain", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbBaselineGuard, DIGIPARA::CH::EnergyFilterBaselineGuard, "Baseline Guard [ns]", trapTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbPileupGuard, DIGIPARA::CH::EnergyFilterPileUpGuard, "Pile-up Guard [ns]", trapTab, iDigi, digi[iDigi]->GetNChannels());
      }

      {//@============== Probe  tab
        QTabWidget * probeTab = new QTabWidget(tab);
        chTabWidget->addTab(probeTab, "Probe");

        SetupComboBoxTab(cbbAnaProbe0, DIGIPARA::CH::WaveAnalogProbe0, "Analog Prob. 0", probeTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbAnaProbe1, DIGIPARA::CH::WaveAnalogProbe1, "Analog Prob. 1", probeTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbDigProbe0, DIGIPARA::CH::WaveDigitalProbe0, "Digital Prob. 0", probeTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbDigProbe1, DIGIPARA::CH::WaveDigitalProbe1, "Digital Prob. 1", probeTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbDigProbe2, DIGIPARA::CH::WaveDigitalProbe2, "Digital Prob. 2", probeTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbDigProbe3, DIGIPARA::CH::WaveDigitalProbe3, "Digital Prob. 3", probeTab, iDigi, digi[iDigi]->GetNChannels(), 2);
      }

      {//@============== Other  tab
        QTabWidget * otherTab = new QTabWidget(tab);
        chTabWidget->addTab(otherTab, "Others");

        SetupComboBoxTab(cbbEventSelector, DIGIPARA::CH::EventSelector, "Event Selector", otherTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbWaveSelector, DIGIPARA::CH::WaveSelector, "Wave Selector", otherTab, iDigi, digi[iDigi]->GetNChannels(), 2 );
        SetupSpinBoxTab(spbEnergySkimLow, DIGIPARA::CH::EnergySkimLowDiscriminator, "Energy Skim Low", otherTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbEnergySkimHigh, DIGIPARA::CH::EnergySkimHighDiscriminator, "Energy Skim High", otherTab, iDigi, digi[iDigi]->GetNChannels());
      }


      {//@============== Trigger  tab
        QTabWidget * triggerTab = new QTabWidget(tab);
        chTabWidget->addTab(triggerTab, "Trigger");

        SetupComboBoxTab(cbbEvtTrigger, DIGIPARA::CH::EventTriggerSource, "Event Trig. Source", triggerTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbWaveTrigger, DIGIPARA::CH::WaveTriggerSource, "Wave Trig. Source", triggerTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbChVetoSrc, DIGIPARA::CH::ChannelVetoSource, "Veto Source", triggerTab, iDigi, digi[iDigi]->GetNChannels(), 2);
        SetupComboBoxTab(cbbCoinMask, DIGIPARA::CH::CoincidenceMask, "Coin. Mask", triggerTab, iDigi, digi[iDigi]->GetNChannels());
        SetupComboBoxTab(cbbAntiCoinMask, DIGIPARA::CH::AntiCoincidenceMask, "Anti-Coin. Mask", triggerTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbCoinLength, DIGIPARA::CH::CoincidenceLength, "Coin. Length [ns]", triggerTab, iDigi, digi[iDigi]->GetNChannels());
        SetupSpinBoxTab(spbADCVetoWidth, DIGIPARA::CH::ADCVetoWidth, "ADC Veto Length [ns]", triggerTab, iDigi, digi[iDigi]->GetNChannels());
      }

      for( int ch = 0; ch < digi[ID]->GetNChannels() + 1; ch++) {
        connect(cbbOnOff[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbOnOff, ch);});
        connect(spbDCOffset[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbDCOffset, ch);});
        connect(spbThreshold[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbThreshold, ch);});
        connect(cbbParity[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbParity, ch);});
        connect(spbRecordLength[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbRecordLength, ch);});
        connect(spbPreTrigger[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbPreTrigger, ch);});
        connect(spbInputRiseTime[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbInputRiseTime, ch);});
        connect(spbTriggerGuard[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbTriggerGuard, ch);});
        connect(cbbLowFilter[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbLowFilter, ch);});
        connect(cbbWaveSource[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveSource, ch);});
        connect(cbbWaveRes[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveRes, ch);});
        connect(cbbWaveSave[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveSave, ch);});

        connect(spbTrapRiseTime[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbTrapRiseTime, ch);});
        connect(spbTrapFlatTop[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbTrapFlatTop, ch);});
        connect(spbTrapPoleZero[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbTrapPoleZero, ch);});
        connect(spbPeaking[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbPeaking, ch);});
        connect(cbbPeakingAvg[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbPeakingAvg, ch);});
        connect(cbbBaselineAvg[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbBaselineAvg, ch);});
        connect(spbFineGain[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbFineGain, ch);});
        connect(spbBaselineGuard[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbBaselineGuard, ch);});
        connect(spbPileupGuard[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbPileupGuard, ch);});
        
        
        connect(cbbAnaProbe0[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbAnaProbe0, ch);});
        connect(cbbAnaProbe1[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbAnaProbe1, ch);});
        connect(cbbDigProbe0[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe0, ch);});
        connect(cbbDigProbe1[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe1, ch);});
        connect(cbbDigProbe2[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe2, ch);});
        connect(cbbDigProbe3[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbDigProbe3, ch);});

        connect(cbbEventSelector[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbEventSelector, ch);});
        connect(cbbWaveSelector[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveSelector, ch);});
        connect(spbEnergySkimLow[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbEnergySkimLow, ch);});
        connect(spbEnergySkimHigh[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbEnergySkimHigh, ch);});

        connect(cbbEvtTrigger[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbEvtTrigger, ch);});
        connect(cbbWaveTrigger[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbWaveTrigger, ch);});
        connect(cbbChVetoSrc[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbChVetoSrc, ch);});
        connect(cbbCoinMask[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbCoinMask, ch);});
        connect(cbbAntiCoinMask[iDigi][ch], &QComboBox::currentIndexChanged, this, [=](){ SyncComboBox(cbbAntiCoinMask, ch);});
        connect(spbCoinLength[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbCoinLength, ch);});
        connect(spbADCVetoWidth[iDigi][ch], &QSpinBox::valueChanged, this, [=](){ SyncSpinBox(spbADCVetoWidth, ch);});
      }

      
      {//@============== Trigger Map  tab
        QTabWidget * triggerMapTab = new QTabWidget(tab);
        chTabWidget->addTab(triggerMapTab, "Trigger Map");

        QGridLayout * triggerLayout = new QGridLayout(triggerMapTab);
        triggerLayout->setAlignment(Qt::AlignCenter);
        triggerLayout->setSpacing(0);
 
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

            //if( i%4 != 0 && j == (i/4)*4) {
            //  bn[i][j]->setStyleSheet("background-color: red;");
            //  bnClickStatus[i][j] = true;
            //}
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
      
    } //=== end of channel group

  } //=== end of tab

  enableSignalSlot = true;

}

DigiSettingsPanel::~DigiSettingsPanel(){

  printf("%s\n", __func__);

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
    leInfo[ID][j]->setText(QString::fromStdString(digi[ID]->GetSettingValue(infoIndex[j].second)));
  } 

  //--------- LED Status
  unsigned int ledStatus = atoi(digi[ID]->GetSettingValue(DIGIPARA::DIG::LED_status).c_str());
  for( int i = 0; i < 19; i++){
    if( (ledStatus >> i) & 0x1 ) {
      LEDStatus[ID][i]->setStyleSheet("background-color:green;");
    }else{
      LEDStatus[ID][i]->setStyleSheet("");
    }
  }

  //--------- ACQ Status
  unsigned int acqStatus = atoi(digi[ID]->GetSettingValue(DIGIPARA::DIG::ACQ_status).c_str());
  for( int i = 0; i < 7; i++){
    if( (acqStatus >> i) & 0x1 ) {
      ACQStatus[ID][i]->setStyleSheet("background-color:green;");
    }else{
      ACQStatus[ID][i]->setStyleSheet("");
    }
  }

  //-------- temperature
  for( int i = 0; i < 8; i++){
    leTemp[ID][i]->setText(QString::fromStdString(digi[ID]->GetSettingValue(DIGIPARA::DIG::TempSensADC[i])));
  }

  //-------- board settings
  ReadCombBoxValue(cbbClockSource[ID], DIGIPARA::DIG::ClockSource);

  QString result = QString::fromStdString(digi[ID]->GetSettingValue(DIGIPARA::DIG::StartSource));
  QStringList resultList = result.remove(QChar(' ')).split("|");
  //qDebug() << resultList << "," << resultList.count();
  for( int j = 0; j < (int) DIGIPARA::DIG::StartSource.GetAnswers().size(); j++){
    ckbStartSource[ID][j]->setChecked(false);
    for( int i = 0; i < resultList.count(); i++){
      //qDebug() << resultList[i] << ", " << QString::fromStdString((DIGIPARA::DIG::StartSource.GetAnswers())[j].first);
      if( resultList[i] == QString::fromStdString((DIGIPARA::DIG::StartSource.GetAnswers())[j].first) ) ckbStartSource[ID][j]->setChecked(true);
    }
  }


  result = QString::fromStdString(digi[ID]->GetSettingValue(DIGIPARA::DIG::GlobalTriggerSource));
  resultList = result.remove(QChar(' ')).split("|");
  for( int j = 0; j < (int) DIGIPARA::DIG::StartSource.GetAnswers().size(); j++){
    ckbGlbTrgSource[ID][j]->setChecked(false);
    for( int i = 0; i < resultList.count(); i++){
      if( resultList[i] == QString::fromStdString((DIGIPARA::DIG::GlobalTriggerSource.GetAnswers())[j].first) ) ckbGlbTrgSource[ID][j]->setChecked(true);
    }
  }

  ReadCombBoxValue(cbbTrgOut[ID], DIGIPARA::DIG::TrgOutMode);
  ReadCombBoxValue(cbbGPIO[ID], DIGIPARA::DIG::GPIOMode);
  ReadCombBoxValue(cbbBusyIn[ID], DIGIPARA::DIG::BusyInSource);
  ReadCombBoxValue(cbbSyncOut[ID], DIGIPARA::DIG::SyncOutMode);
  ReadCombBoxValue(cbbAutoDisarmAcq[ID], DIGIPARA::DIG::EnableAutoDisarmACQ);
  ReadCombBoxValue(cbbStatEvents[ID], DIGIPARA::DIG::EnableStatisticEvents);
  ReadCombBoxValue(cbbBdVetoPolarity[ID], DIGIPARA::DIG::BoardVetoPolarity);
  ReadCombBoxValue(cbbBoardVetoSource[ID], DIGIPARA::DIG::BoardVetoSource);
  ReadCombBoxValue(cbbIOLevel[ID], DIGIPARA::DIG::IO_Level);

  result = QString::fromStdString(digi[ID]->GetSettingValue(DIGIPARA::DIG::BoardVetoWidth));
  spbBdVetoWidth[ID]->setValue(result.toInt());
  
  result = QString::fromStdString(digi[ID]->GetSettingValue(DIGIPARA::DIG::RunDelay));
  spbRunDelay[ID]->setValue(result.toInt());
  
  result = QString::fromStdString(digi[ID]->GetSettingValue(DIGIPARA::DIG::VolatileClockOutDelay));
  dsbVolatileClockOutDelay[ID]->setValue(result.toDouble());
  
  result = QString::fromStdString(digi[ID]->GetSettingValue(DIGIPARA::DIG::PermanentClockOutDelay));
  dsbClockOutDelay[ID]->setValue(result.toDouble());

  //------------- test pulse
  result = QString::fromStdString(digi[ID]->GetSettingValue(DIGIPARA::DIG::TestPulsePeriod));
  dsbTestPuslePeriod[ID]->setValue(result.toDouble());
  result = QString::fromStdString(digi[ID]->GetSettingValue(DIGIPARA::DIG::TestPulseWidth));
  dsbTestPusleWidth[ID]->setValue(result.toDouble());
  result = QString::fromStdString(digi[ID]->GetSettingValue(DIGIPARA::DIG::TestPulseLowLevel));
  spbTestPusleLowLevel[ID]->setValue(result.toInt());
  result = QString::fromStdString(digi[ID]->GetSettingValue(DIGIPARA::DIG::TestPulseHighLevel));
  spbTestPusleHighLevel[ID]->setValue(result.toInt());

  
  enableSignalSlot = true;

}
//^###########################################################################

void DigiSettingsPanel::SetStartSource(){
  if( !enableSignalSlot ) return;

  std::string value = "";
  for( int i = 0; i < (int) DIGIPARA::DIG::StartSource.GetAnswers().size(); i++){
    if( ckbStartSource[ID][i]->isChecked() ){
      //printf("----- %s \n", DIGIPARA::DIG::StartSource.GetAnswers()[i].first.c_str());
      if( value != "" ) value += " | ";
      value += DIGIPARA::DIG::StartSource.GetAnswers()[i].first;
    }
  }

  printf("================ %s\n", value.c_str());
  digi[ID]->WriteValue(DIGIPARA::DIG::StartSource, value);

}

void DigiSettingsPanel::SetGlobalTriggerSource(){
  if( !enableSignalSlot ) return;

  std::string value = "";
  for( int i = 0; i < (int) DIGIPARA::DIG::GlobalTriggerSource.GetAnswers().size(); i++){
    if( ckbGlbTrgSource[ID][i]->isChecked() ){
      //printf("----- %s \n", DIGIPARA::DIG::StartSource.GetAnswers()[i].first.c_str());
      if( value != "" ) value += " | ";
      value += DIGIPARA::DIG::GlobalTriggerSource.GetAnswers()[i].first;
    }
  }

  printf("================ %s\n", value.c_str());
  digi[ID]->WriteValue(DIGIPARA::DIG::GlobalTriggerSource, value);

}

//^###########################################################################
void DigiSettingsPanel::SetupShortComboBox(QComboBox *cbb, Reg para){
  for( int i = 0 ; i < (int) para.GetAnswers().size(); i++){
    cbb->addItem(QString::fromStdString((para.GetAnswers())[i].second), 
                QString::fromStdString((para.GetAnswers())[i].first));
  }
}

void DigiSettingsPanel::SetupComboBox(QComboBox *&cbb, const Reg para, int ch_index, bool isMaster, QString labelTxt, QGridLayout *layout, int row, int col, int srow, int scol){
  QLabel * lb = new QLabel(labelTxt, this); 
  layout->addWidget(lb, row, col);
  lb->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  cbb = new QComboBox(this);
  layout->addWidget(cbb, row, col+1, srow, scol);
  for( int i = 0 ; i < (int) para.GetAnswers().size(); i++){
    cbb->addItem(QString::fromStdString((para.GetAnswers())[i].second), QString::fromStdString((para.GetAnswers())[i].first));
  }
  if( isMaster && para.GetType() == TYPE::CH ) cbb->addItem("");
  connect(cbb, &QComboBox::currentIndexChanged, this, [=](){
    if( !enableSignalSlot ) return;
    //printf("%s %d  %s \n", para.GetPara().c_str(), ch_index, cbb->currentData().toString().toStdString().c_str());
    digi[ID]->WriteValue(para, cbb->currentData().toString().toStdString(), ch_index);
  });
}

void DigiSettingsPanel::SetupSpinBox(QSpinBox *&spb, const Reg para, int ch_index, QString labelTxt, QGridLayout *layout, int row, int col, int srow, int scol){
  QLabel * lb = new QLabel(labelTxt, this); 
  layout->addWidget(lb, row, col);
  lb->setAlignment(Qt::AlignRight| Qt::AlignCenter);
  spb = new QSpinBox(this);
  spb->setMinimum(atoi( para.GetAnswers()[0].first.c_str()));
  spb->setMaximum(atoi( para.GetAnswers()[1].first.c_str()));
  layout->addWidget(spb, row, col + 1, srow, scol);
  connect(spb, &QSpinBox::valueChanged, this, [=](){
    if( !enableSignalSlot ) return;
    //printf("%s %d  %d \n", para.GetPara().c_str(), ch_index, spb->value());
    digi[ID]->WriteValue(para, std::to_string(spb->value()), ch_index);
  });
}

void DigiSettingsPanel::SyncComboBox(QComboBox *(&cbb)[][MaxNumberOfChannel + 1], int ch){
  if( !enableSignalSlot ) return;

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

void DigiSettingsPanel::SyncSpinBox(QSpinBox *(&spb)[][MaxNumberOfChannel+1], int ch){
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

    enableSignalSlot = false;
    if( count != nCh ){
       spb[ID][nCh]->setValue(-999);
    }else{
       spb[ID][nCh]->setValue(value);
    }
    enableSignalSlot = true;
  }
}

void DigiSettingsPanel::SetupSpinBoxTab(QSpinBox *(&spb)[][MaxNumberOfChannel+1], const Reg para, QString text, QTabWidget *tabWidget, int iDigi, int nChannel){
  QWidget * tabPage = new QWidget(this); tabWidget->addTab(tabPage, text);
  QGridLayout * allLayout = new QGridLayout(tabPage); 
  //allLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  allLayout->setAlignment(Qt::AlignTop);
  allLayout->setHorizontalSpacing(10);
  allLayout->setVerticalSpacing(0);
  for( int ch = 0; ch < nChannel; ch++){
    SetupSpinBox(spb[iDigi][ch], para, ch,  "ch-"+QString::number(ch)+ "  ", allLayout, ch/4, ch%4 * 2);
  }
}

void DigiSettingsPanel::SetupComboBoxTab(QComboBox *(&cbb)[][MaxNumberOfChannel + 1], const Reg para, QString text, QTabWidget *tabWidget, int iDigi, int nChannel, int nCol){
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

void DigiSettingsPanel::ReadCombBoxValue(QComboBox *cbb, const Reg para){
  QString result = QString::fromStdString(digi[ID]->GetSettingValue(para));
  //printf("%s === %s, %d, %p\n", __func__, result.toStdString().c_str(), ID, cbb);
  int index = cbb->findData(result);
  if( index >= 0 && index < cbb->count()) {
    cbb->setCurrentIndex(index);
  }else{
    qDebug() << result;
  }
}
