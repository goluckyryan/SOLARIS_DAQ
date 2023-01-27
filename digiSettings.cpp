#include "digiSettings.h"

#include <QLabel>

DigiSettings::DigiSettings(Digitizer2Gen * digi, unsigned short nDigi, QWidget * parent) : QWidget(parent){

  qDebug() << "DigiSettings constructor";

  setWindowTitle("Digitizers Settings");
  setGeometry(200, 50, 1000, 1000);
  //setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);


  this->digi = digi;
  this->nDigi = nDigi;

  this->nDigi = 1;


  std::vector<std::vector<std::string>> info = {{"Serial Num : ", "/par/SerialNum"},
                                                {"IP : ", "/par/IPAddress"},
                                                {"Model Name : ", "/par/ModelName"},
                                                {"FPGA version : ", "/par/FPGA_FwVer"},
                                                {"DPP Type : ", "/par/FwType"},
                                                {"CUP version : ", "/par/cupver"},
                                                {"ADC bits : ", "/par/ADC_Nbit"},
                                                {"ADC rate [Msps] : ", "/par/ADC_SamplRate"},
                                                {"Num. of Channel : ", "/par/NumCh"}
                                               };

  QVBoxLayout * mainLayout = new QVBoxLayout(this);
  this->setLayout(mainLayout);
  QTabWidget * tabWidget = new QTabWidget(this);
  mainLayout->addWidget(tabWidget);

  

  //============ Tab for each digitizer
  for(unsigned short i = 0; i < this->nDigi; i++){
  

    QWidget * tab = new QWidget(tabWidget);
    
    QScrollArea * scrollArea = new QScrollArea;
    scrollArea->setWidget(tab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    //tabWidget->addTab(scrollArea, "Digi-" + QString::number(digi->GetSerialNumber()));
    tabWidget->addTab(scrollArea, "Digi-dummy");

    QGridLayout *tabLayout = new QGridLayout(tab);
    tab->setLayout(tabLayout);

    //-------- Group of Digitizer Info
    QGroupBox * infoBox = new QGroupBox("Board Info", tab);
    QGridLayout * infoLayout = new QGridLayout(infoBox);
    infoBox->setLayout(infoLayout);
    tabLayout->addWidget(infoBox, 0, 0);

    for( unsigned short j = 0; j < (unsigned short) info.size(); j++){
      QLabel * lab = new QLabel(QString::fromStdString(info[j][0]), tab);
      lab->setAlignment(Qt::AlignRight);
      QLineEdit * txt = new QLineEdit(tab);
      txt->setReadOnly(true);
      //txt->setText(QString::fromStdString(digi->ReadValue(info[j][1].c_str())));
      //txt->setStyleSheet("color: black;");
      infoLayout->addWidget(lab, j%3, 2*(j/3));
      infoLayout->addWidget(txt, j%3, 2*(j/3) +1);
    }
    
    //------- Group Board status
    QGroupBox * statusBox = new QGroupBox("Board Status", tab);
    QGridLayout * statusLayout = new QGridLayout(statusBox);
    statusBox->setLayout(statusLayout);
    tabLayout->addWidget(statusBox, 1, 0);

    //------- Group digitizer settings
    QGroupBox * digiBox = new QGroupBox("Board Settings", tab);
    QGridLayout * boardLayout = new QGridLayout(digiBox);
    digiBox->setLayout(boardLayout);
    tabLayout->addWidget(digiBox, 2, 0);
    
    {
      int rowId = 0;
      //-------------------------------------
      QPushButton * bnResetBd = new QPushButton("Reset Board", tab);
      boardLayout->addWidget(bnResetBd, rowId, 0, 1, 2);
      
      QPushButton * bnDefaultSetting = new QPushButton("Set Default Settings", tab);
      boardLayout->addWidget(bnDefaultSetting, rowId, 2, 1, 2);

      //-------------------------------------
      rowId ++;
      QLabel * lbStartSource = new QLabel("Start Source :", tab);
      lbStartSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbStartSource, rowId, 0);

      QCheckBox * cbStartSource1 = new QCheckBox("EncodedClkIn", tab);
      boardLayout->addWidget(cbStartSource1, rowId, 1);
      QCheckBox * cbStartSource2 = new QCheckBox("SIN Level", tab);
      boardLayout->addWidget(cbStartSource2, rowId, 2);
      QCheckBox * cbStartSource3 = new QCheckBox("SIN Edge", tab);
      boardLayout->addWidget(cbStartSource3, rowId, 3);
      QCheckBox * cbStartSource4 = new QCheckBox("LVDS", tab);
      boardLayout->addWidget(cbStartSource4, rowId, 4);

      //-------------------------------------
      rowId ++;
      QLabel * lbGlobalTrgSource = new QLabel("Global Trigger Source :", tab);
      lbGlobalTrgSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbGlobalTrgSource, rowId, 0);

      QCheckBox * cbGlbTrgSource1 = new QCheckBox("Trg-IN", tab);
      boardLayout->addWidget(cbGlbTrgSource1, rowId, 1);
      QCheckBox * cbGlbTrgSource2 = new QCheckBox("SwTrg", tab);
      boardLayout->addWidget(cbGlbTrgSource2, rowId, 2);
      QCheckBox * cbGlbTrgSource3 = new QCheckBox("LVDS", tab);
      boardLayout->addWidget(cbGlbTrgSource3, rowId, 3);
      QCheckBox * cbGlbTrgSource4 = new QCheckBox("GPIO", tab);
      boardLayout->addWidget(cbGlbTrgSource4, rowId, 4);
      QCheckBox * cbGlbTrgSource5 = new QCheckBox("TestPulse", tab);
      boardLayout->addWidget(cbGlbTrgSource5, rowId, 5);

      //-------------------------------------
      rowId ++;
      QLabel * lbTrgOutMode = new QLabel("Trg-OUT Mode :", tab);
      lbTrgOutMode->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbTrgOutMode, rowId, 0);

      QComboBox * comTrgOut = new QComboBox(tab);
      boardLayout->addWidget(comTrgOut, rowId, 1, 1, 2);
      comTrgOut->addItem("Disabled");
      comTrgOut->addItem("TRG-IN");
      comTrgOut->addItem("SwTrg");
      comTrgOut->addItem("LVDS");
      comTrgOut->addItem("Run");
      comTrgOut->addItem("TestPulse");
      comTrgOut->addItem("Busy");
      comTrgOut->addItem("Fixed 0");
      comTrgOut->addItem("Fixed 1");
      comTrgOut->addItem("SyncIn signal");
      comTrgOut->addItem("S-IN signal");
      comTrgOut->addItem("GPIO signal");
      comTrgOut->addItem("Accepted triggers");
      comTrgOut->addItem("Trigger Clock signal");

      //-------------------------------------
      rowId ++;
      QLabel * lbGPIOMode = new QLabel("GPIO Mode :", tab);
      lbGPIOMode->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbGPIOMode, rowId, 0);

      QComboBox * comGPIO = new QComboBox(tab);
      boardLayout->addWidget(comGPIO, rowId, 1, 1, 2);
      comGPIO->addItem("Disabled");
      comGPIO->addItem("TRG-IN");
      comGPIO->addItem("S-IN");
      comGPIO->addItem("LVDS");
      comGPIO->addItem("SwTrg");
      comGPIO->addItem("Run");
      comGPIO->addItem("TestPulse");
      comGPIO->addItem("Busy");
      comGPIO->addItem("Fixed 0");
      comGPIO->addItem("Fixed 1");

      //-------------------------------------
      rowId ++;
      QLabel * lbBusyInSource = new QLabel("Busy In Source :", tab);
      lbBusyInSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbBusyInSource, rowId, 0);

      QComboBox * comBusyIn = new QComboBox(tab);
      boardLayout->addWidget(comBusyIn, rowId, 1, 1, 2);
      comBusyIn->addItem("Disabled");
      comBusyIn->addItem("GPIO");
      comBusyIn->addItem("LVDS");
      comBusyIn->addItem("LVDS");

      //-------------------------------------
      rowId ++;
      QLabel * lbSyncOutMode = new QLabel("Sync Out mode :", tab);
      lbSyncOutMode->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbSyncOutMode, rowId, 0);

      QComboBox * comSyncOut = new QComboBox(tab);
      boardLayout->addWidget(comSyncOut, rowId, 1, 1, 2);
      comSyncOut->addItem("Disabled");
      comSyncOut->addItem("SyncIn");
      comSyncOut->addItem("TestPulse");
      comSyncOut->addItem("Internal 62.5 MHz Clock");
      comSyncOut->addItem("Run");

      //-------------------------------------
      rowId ++;
      QLabel * lbBoardVetoSource = new QLabel("Board Veto Source :", tab);
      lbBoardVetoSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbBoardVetoSource, rowId, 0);

      QComboBox * comBoardVetoSource = new QComboBox(tab);
      boardLayout->addWidget(comBoardVetoSource, rowId, 1, 1, 2);
      comBoardVetoSource->addItem("Disabled");
      comBoardVetoSource->addItem("S-In");
      comBoardVetoSource->addItem("LVSD");
      comBoardVetoSource->addItem("GPIO");

      QLabel * lbBdVetoWidth = new QLabel("Board Veto Width [ns] : ", tab);
      lbBdVetoWidth->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbBdVetoWidth, rowId, 3, 1, 2);

      QSpinBox * sbBdVetoWidth = new QSpinBox(tab); // may be QDoubleSpinBox
      sbBdVetoWidth->setMinimum(0);
      sbBdVetoWidth->setSingleStep(20);
      boardLayout->addWidget(sbBdVetoWidth, rowId, 5);

      QComboBox * comBdVetoPolarity = new QComboBox(tab);
      boardLayout->addWidget(comBdVetoPolarity, rowId, 6);
      comBdVetoPolarity->addItem("High");
      comBdVetoPolarity->addItem("Low");
      
      //-------------------------------------
      rowId ++;
      QLabel * lbRunDelay = new QLabel("Run Delay [ns] :", tab);
      lbRunDelay->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbRunDelay, rowId, 0);

      QSpinBox * sbRunDelay = new QSpinBox(tab);
      sbRunDelay->setMinimum(0);
      sbRunDelay->setMaximum(524280);
      sbRunDelay->setSingleStep(20);
      boardLayout->addWidget(sbRunDelay, rowId, 1);

      //-------------------------------------
      rowId ++;
      QLabel * lbAutoDisarmAcq = new QLabel("Auto disarm ACQ :", tab);
      lbAutoDisarmAcq->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbAutoDisarmAcq, rowId, 0);

      QComboBox * comAutoDisarmAcq = new QComboBox(tab);
      boardLayout->addWidget(comAutoDisarmAcq, rowId, 1);
      comAutoDisarmAcq->addItem("Enabled");
      comAutoDisarmAcq->addItem("Disabled");

      //-------------------------------------
      rowId ++;
      QLabel * lbClockOutDelay = new QLabel("Clock Out Delay [ps] :", tab);
      lbClockOutDelay->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbClockOutDelay, rowId, 0);

      QDoubleSpinBox * dsbClockOutDelay = new QDoubleSpinBox(tab);
      dsbClockOutDelay->setMinimum(-1888.888);
      dsbClockOutDelay->setMaximum(1888.888);
      dsbClockOutDelay->setValue(0);
      boardLayout->addWidget(dsbClockOutDelay, rowId, 1);

      //-------------------------------------
      rowId ++;
      QLabel * lbIOLevel = new QLabel("IO Level :", tab);
      lbIOLevel->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbIOLevel, rowId, 0);

      QComboBox * comIOLevel = new QComboBox(tab);
      boardLayout->addWidget(comIOLevel, rowId, 1, 1, 2);
      comIOLevel->addItem("NIM (0 = 0V, 1 = -0.8V)");
      comIOLevel->addItem("TTL (0 = 0V, 1 =  3.3V)");

      //-------------------------------------- Test pulse

    }

    //------- Group channel settings
    QGroupBox * chBox = new QGroupBox("Channel Settings", tab);
    QGridLayout * chLayout = new QGridLayout(chBox);
    chBox->setLayout(chLayout);
    tabLayout->addWidget(chBox, 0, 1, 10, 1);
    chLayout->setVerticalSpacing(0);

    //for( unsigned short ch = 0; ch < digi->GetNChannels(); ch++){
    for( unsigned short ch = 0; ch < 64; ch++){
      QLabel * labCh = new QLabel(QString::number(ch), tab);
      labCh->setAlignment(Qt::AlignRight);
      chLayout->addWidget(labCh, ch, 0);

      QCheckBox * cbCh = new QCheckBox(tab);
      //std::string onOff = digi->ReadValue(("/ch/" + std::to_string(ch)+ "/par/ChEnable").c_str());
      //qDebug() << QString::fromStdString(std::to_string(ch)  +  ", " + onOff);
      //if( onOff == "True"){
      //  cbCh->setChecked(true);
      //}else{
      //  cbCh->setChecked(false);
      //}

      chLayout->addWidget(cbCh, ch, 1);

    }

    //------- Group trigger settings
    QGroupBox * triggerBox = new QGroupBox("Trigger Map", tab);
    QGridLayout * triggerLayout = new QGridLayout(triggerBox);
    triggerBox->setLayout(triggerLayout);
    tabLayout->addWidget(triggerBox, 3, 0);
    {

      triggerLayout->setHorizontalSpacing(0);
      triggerLayout->setVerticalSpacing(0);

      QLabel * instr = new QLabel("Reading: Column (C) represents a trigger channel for Row (R) channel.\nFor example, R3C1 = ch-3 trigger source is ch-1.\n", tab);
      triggerLayout->addWidget(instr, 0, 0, 1, 64+15);

      QSignalMapper * triggerMapper = new QSignalMapper(tab);

      int count = 0;
      int rowID = 1;
      int colID = 0;
      for(int i = 0; i < 64; i++){
        colID = 0;
        for(int j = 0; j < 64; j++){
          
          bn[i][j] = new QPushButton(tab);
          bn[i][j]->setFixedSize(QSize(10,10));
          bnClickStatus[i][j] = false;

          if( i%4 != 0 && j == (i/4)*4) {
            bn[i][j]->setStyleSheet("background-color: red;");
            bnClickStatus[i][j] = true;
          }

          triggerMapper->setMapping(bn[i][j], 100*i+j);
          
          connect(bn[i][j], SIGNAL(clicked()), triggerMapper, SLOT(map()));

          count++;

          triggerLayout->addWidget(bn[i][j], rowID, colID);

          colID ++;

          if( j%4 == 3 && j!= 63){
            QFrame * vSeparator = new QFrame(tab);
            vSeparator->setFrameShape(QFrame::VLine);
            triggerLayout->addWidget(vSeparator, rowID, colID);
            colID++;
          }
        }

        rowID++;

        if( i%4 == 3 && i != 63){
          QFrame * hSeparator = new QFrame(tab);
          hSeparator->setFrameShape(QFrame::HLine);
          triggerLayout->addWidget(hSeparator, rowID, 0, 1, 64 + 15);
          rowID++;
        }


      }

      //connect(triggerMapper, &QSignalMapper::mappedString, this, &DigiSettings::onTriggerClick);

      connect(triggerMapper, SIGNAL(mappedInt(int)), this, SLOT(onTriggerClick(int)));
    }


  } //=== end of tab


}

DigiSettings::~DigiSettings(){

}