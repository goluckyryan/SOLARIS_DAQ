#include "SOLARISpanel.h"

#include <QFile>
#include <QLabel>
#include <QSet>
#include <QList>
#include <QFrame>
#include <QFileDialog>

#define NCOL 10 // number of column

#define ChStartIndex 2 // the index of detIDList for channel

std::vector<Reg> SettingItems = {PHA::CH::TriggerThreshold, PHA::CH::DC_Offset};

SOLARISpanel::SOLARISpanel(Digitizer2Gen **digi, unsigned short nDigi, 
                          QString analysisPath,
                          std::vector<std::vector<int>> mapping, 
                          QStringList detType, 
                          QStringList detGroupName,
                          std::vector<int> detGroupID,
                          std::vector<int> detMaxID, 
                          QWidget *parent) : QWidget(parent){

  setWindowTitle("SOLARIS Settings");
  setGeometry(0, 0, 1350, 800);

  this->digi = digi;
  this->nDigi = nDigi;
  if( this->nDigi > MaxNumberOfDigitizer ) {
    this->nDigi = MaxNumberOfChannel;
    qDebug() << "Please increase the MaxNumberOfChannel";
  }
  this->mapping = mapping;
  this->detType = detType;
  this->detMaxID = detMaxID;
  this->detGroupID = detGroupID;
  this->detGroupName = detGroupName;
  this->digiSettingPath = analysisPath + "/working/Settings/";

  //Check number of detector type; Array 0-199, Recoil 200-299, other 300-
  int nDetType = detType.size();
  for( int k = 0 ; k < nDetType; k++) nDetinType.push_back(0);

  std::vector<int> condenGroupID = detGroupID;
  std::sort(condenGroupID.begin(), condenGroupID.end()); // sort the detGroupID
  auto last = std::unique(condenGroupID.begin(), condenGroupID.end());
  condenGroupID.erase(last, condenGroupID.end());
  int nDetGroup = condenGroupID.size();
  for( int k = 0 ; k < nDetGroup; k++) nDetinGroup.push_back(0);


  QList<QList<int>> detIDListTemp; // store { detGroup, detID,  (Digi << 8 ) + ch}, making mapping into 1-D array to be consolidated
  printf("################################# \n");
  for( int i = 0; i < (int) mapping.size() ; i++){
    for( int j = 0; j < (int) mapping[i].size(); j++ ){
      printf("%4d,", mapping[i][j]);
      if( mapping[i][j] >= 0 ) {
        int groupID = FindDetGroup(mapping[i][j]);
        int typeID = FindDetTypeID(mapping[i][j]);
        QList<int> haha ;
        haha <<  groupID <<  mapping[i][j] << ((i << 8 ) + j);
        detIDListTemp <<  haha;
        nDetinType[typeID] ++;
        nDetinGroup[groupID] ++;
      }
      if( j % 16 == 15 ) printf("\n");
    }
    printf("------------------ \n");
  }

  //consolidate detIDListTemp --> 2D array of (detID, (Digi << 8) +  ch, +.... )
  //for example, {2, 0, 0}, {2, 100, 1}, {2, 200, 2}--> {2, 0, 0, 1, 2};
  //for example, {2, 1, 3}, {2, 101, 4}, {2, 201, 5}--> {2, 0, 3, 4, 5};
  detIDList.clear();
  bool repeated = false;
  for( int i = 0; i < detIDListTemp.size(); i++ ){
    repeated = false;
    if( detIDList.size() == 0 ){
      detIDList << detIDListTemp[i];
      continue;
    }
    for( int j = 0; j < detIDList.size() ; j++){
      if( detIDList[j][0] == detIDListTemp[i][0] ) { // same group

        int type1 = FindDetTypeID(detIDList[j][1]); 
        int type2 = FindDetTypeID(detIDListTemp[i][1]); 

        int low1 = (type1 == 0 ? 0 : detMaxID[type1-1]);
        int low2 = (type2 == 0 ? 0 : detMaxID[type2-1]);

        int detID1 = detIDList[j][1] - low1;
        int detID2 = detIDListTemp[i][1] - low2;

        if( detID1 == detID2) {
          repeated = true;
          detIDList[j] << detIDListTemp[i][2];
          break;
        }
      }
    }
    if( !repeated ) {
      detIDList << detIDListTemp[i];
    }
  }

  //---- sort detIDList;
  std::sort(detIDList.begin(), detIDList.end(), [](const QList<int>& a, const QList<int>& b) {
    return a.at(1) < b.at(1);
  });

  //------------- display detector summary
  //qDebug() << detIDList;
  printf("---------- num. of det. Type : %d\n", nDetType);
  for( int i =0; i < nDetType; i ++ ) {
    detType[i].remove(' ');
    printf(" Type-%d (%6s) : %3d det.  (%3d - %3d)\n", i, detType[i].toStdString().c_str(), nDetinType[i], (i==0 ? 0 : detMaxID[i-1]), detMaxID[i]-1);
  }
  printf("---------- num. of det. Group : %d\n", nDetGroup);
  for( int i =0; i < nDetGroup; i++){
    printf(" Group-%d (%10s) : %3d det.\n", i, detGroupName[i].toStdString().c_str(), nDetinGroup[i]);
  }

  //---------- Set Panel
  QGridLayout * mainLayout = new QGridLayout(this); this->setLayout(mainLayout);

  ///=================================
  int rowIndex = 0 ;
  QPushButton * bnRefresh = new QPushButton("Refresh Settings", this);
  connect(bnRefresh, &QPushButton::clicked, this, &SOLARISpanel::RefreshSettings );
  mainLayout->addWidget(bnRefresh, rowIndex, 0, 1, 2);

  QPushButton * bnSaveSetting = new QPushButton("Save Settings", this);
  connect(bnSaveSetting, &QPushButton::clicked, this, &SOLARISpanel::SaveSettings);
  mainLayout->addWidget(bnSaveSetting, rowIndex, 2, 1, 2);  
  
  QPushButton * bnLoadSetting = new QPushButton("Load Settings", this);
  connect(bnLoadSetting, &QPushButton::clicked, this, &SOLARISpanel::LoadSettings);
  mainLayout->addWidget(bnLoadSetting, rowIndex, 4, 1, 2);

  QLabel * lbCoinTime = new QLabel("Coin. Time (all ch.) [ns]", this);
  lbCoinTime->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  mainLayout->addWidget(lbCoinTime, rowIndex, 6, 1, 2);

  sbCoinTime = new RSpinBox(this);
  sbCoinTime->setMinimum(-1);
  sbCoinTime->setMaximum(atof(PHA::CH::CoincidenceLength.GetAnswers()[1].first.c_str()));
  sbCoinTime->setSingleStep(atof(PHA::CH::CoincidenceLength.GetAnswers()[2].first.c_str()));
  sbCoinTime->setDecimals(0);
  sbCoinTime->SetToolTip(atof(PHA::CH::CoincidenceLength.GetAnswers()[1].first.c_str()));
  mainLayout->addWidget(sbCoinTime, rowIndex, 8, 1, 2);

  
  connect(sbCoinTime, &RSpinBox::valueChanged, this, [=](){
    if( !enableSignalSlot ) return;
    sbCoinTime->setStyleSheet("color:blue;");
  });

  connect(sbCoinTime, &RSpinBox::returnPressed, this, [=](){
    if( !enableSignalSlot ) return;
    //printf("%s %d  %d \n", para.GetPara().c_str(), index, spb->value());
    if( sbCoinTime->decimals() == 0 && sbCoinTime->singleStep() != 1) {
      double step = sbCoinTime->singleStep();
      double value = sbCoinTime->value();
      sbCoinTime->setValue( (std::round(value/step) * step) );
    }

    for(int i = 0; i < (int) mapping.size(); i ++){
      if( i >= nDigi || digi[i]->IsDummy() || !digi[i]->IsConnected() ) return;
      QString msg;
      msg = QString::fromStdString(PHA::CH::CoincidenceLength.GetPara()) + "|DIG:"+ QString::number(digi[i]->GetSerialNumber());
      msg += ",CH:All = " + QString::number(sbCoinTime->value());
      if( digi[i]->WriteValue(PHA::CH::CoincidenceLength, std::to_string(sbCoinTime->value()))){
        SendLogMsg(msg + "|OK.");
        sbCoinTime->setStyleSheet("");
      }else{
        SendLogMsg(msg + "|Fail.");
        sbCoinTime->setStyleSheet("color:red;");
      }
    }
    UpdatePanelFromMemory();
  });
  
  ///=================================
  rowIndex ++;
  QLabel * info = new QLabel("Only simple trigger is avalible. For complex trigger scheme, please use the setting panel.", this);
  mainLayout->addWidget(info, rowIndex, 0, 1, 4);

  ///=================================
  rowIndex ++;
  QTabWidget * tabWidget = new QTabWidget(this); mainLayout->addWidget(tabWidget, rowIndex, 0, 1, 10);
  for( int detGroup = 0; detGroup < nDetGroup; detGroup ++ ){

    QTabWidget * tabSetting = new QTabWidget(tabWidget);
    tabWidget->addTab(tabSetting, detGroupName[detGroup]);

    for( int SettingID = 0; SettingID < (int) SettingItems.size(); SettingID ++){

      QScrollArea * scrollArea = new QScrollArea(this);
      scrollArea->setWidgetResizable(true);
      scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      tabSetting->addTab(scrollArea,  QString::fromStdString(SettingItems[SettingID]));

      QWidget * tab = new QWidget(tabSetting);
      scrollArea->setWidget(tab);
      
      QGridLayout * layout = new QGridLayout(tab);
      layout->setAlignment(Qt::AlignLeft|Qt::AlignTop);
      layout->setSpacing(0);

      chkAll[detGroup][SettingID] = new QCheckBox("Set for all", tab);
      layout->addWidget(chkAll[detGroup][SettingID], 0, 0);

      connect(chkAll[detGroup][SettingID], &QCheckBox::stateChanged, this, [=](bool state){
        bool found = false;
        for(int i = 0; i < detIDList.size(); i++){
          if( detIDList[i][0] != detGroup ) continue;
          if( found == false ){
            found = true;
            continue;
          }else{
            groupBox[detGroup][SettingID][detIDList[i][1]]->setEnabled(!state);
          }
        }
      });
      
      QFrame *line = new QFrame(tab);
      line->setFrameShape(QFrame::HLine);
      line->setFrameShadow(QFrame::Sunken);
      line->setFixedHeight(10);
      layout->addWidget(line, 1, 0, 1, NCOL);

      for(int i = 0; i < detIDList.size(); i++){
        if( detIDList[i][0] != detGroup ) continue;
        CreateDetGroup(SettingID, detIDList[i], layout, i/NCOL +  2, i%NCOL);
      }

    }
  }

  enableSignalSlot = true;

}

SOLARISpanel::~SOLARISpanel(){

}

//^######################################################################
int SOLARISpanel::FindDetTypeID(int detID){
  for( int i = 0; i < (int) detType.size(); i++){
    int lowID = (i == 0) ? 0 : detMaxID[i-1];
    if( lowID <= detID && detID < detMaxID[i]) {
      return i;
    }
  }
  return -1;
}

int SOLARISpanel::FindDetGroup(int detID){
  int typeID = FindDetTypeID(detID);
  if( typeID == -1) return -999;
  return detGroupID[typeID];
}

void SOLARISpanel::CreateDetGroup(int SettingID, QList<int> detIDArray, QGridLayout * &layout, int row, int col){

  int detGroup = detIDArray[0];
  int detID = detIDArray[1];

  groupBox[detGroup][SettingID][detID] = new QGroupBox("Det-" + QString::number(detID), this);
  groupBox[detGroup][SettingID][detID]->setFixedWidth(130);
  groupBox[detGroup][SettingID][detID]->setAlignment(Qt::AlignCenter);
  QGridLayout * layout0 = new QGridLayout(groupBox[detGroup][SettingID][detID]);
  layout0->setSpacing(0);
  layout0->setAlignment(Qt::AlignLeft);

  //@======================================== SpinBox and Display
  bool isDisableDetector = false;

  int nChInGroupBox = 0;
  for( int ppp = ChStartIndex; ppp < detIDArray.size(); ppp ++){

    int chIndex = ppp - ChStartIndex;
    isDisableDetector = false;
    nChInGroupBox ++;

    int digiID = (detIDArray[ppp] >> 8 );
    int chID = (detIDArray[ppp] & 0xFF);

    int typeID = FindDetTypeID(mapping[digiID][chID]);

    QLabel * lb  = new QLabel(detType[typeID].remove(' '), this);  
    layout0->addWidget(lb, 2*chIndex, 0, 2, 1);

    chkOnOff[SettingID][digiID][chID] = new QCheckBox(this);
    layout0->addWidget(chkOnOff[SettingID][digiID][chID], 2*chIndex, 1, 2, 1);

    leDisplay[SettingID][digiID][chID] = new QLineEdit(this);
    leDisplay[SettingID][digiID][chID]->setFixedWidth(70);
    layout0->addWidget(leDisplay[SettingID][digiID][chID], 2*chIndex, 2);

    sbSetting[SettingID][digiID][chID] = new RSpinBox(this);
    if( digiID < nDigi ) sbSetting[SettingID][digiID][chID]->setToolTip( "Digi-" + QString::number(digi[digiID]->GetSerialNumber()) + ", Ch-" + QString::number(chID));
    sbSetting[SettingID][digiID][chID]->setToolTipDuration(-1);
    sbSetting[SettingID][digiID][chID]->setFixedWidth(70);

    sbSetting[SettingID][digiID][chID]->setMinimum(atoi(SettingItems[SettingID].GetAnswers()[0].first.c_str()));
    sbSetting[SettingID][digiID][chID]->setMaximum(atoi(SettingItems[SettingID].GetAnswers()[1].first.c_str()));
    sbSetting[SettingID][digiID][chID]->setSingleStep(atoi(SettingItems[SettingID].GetAnswers()[2].first.c_str()));

    layout0->addWidget(sbSetting[SettingID][digiID][chID], 2*chIndex+1, 2);

    if( digiID >= nDigi || chID >= digi[digiID]->GetNChannels() ) {
      leDisplay[SettingID][digiID][chID]->setEnabled(false);
      sbSetting[SettingID][digiID][chID]->setEnabled(false);    
      chkOnOff[SettingID][digiID][chID]->setEnabled(false); 
      isDisableDetector = true;   
    }

    ///========================= for SpinBox
    RSpinBox * spb = sbSetting[SettingID][digiID][chID];
    const Reg para = SettingItems[SettingID];

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

      if( chkAll[detGroup][SettingID]->isChecked() ){
        for(int k = 0; k < detIDList.size() ; k++){
          if( detGroup == detIDList[k][0] ){
            for( int h = ChStartIndex; h < detIDList[k].size() ; h++){
              if( h != chIndex + ChStartIndex) continue;
              int digiK = (detIDList[k][h] >> 8);
              if( digiK >= nDigi ) continue;
              int index = (detIDList[k][h] & 0xFF);

              QString msg;
              msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[digiK]->GetSerialNumber());
              msg += ",CH:" + QString::number(index) + "(" + detType[typeID] + ")";
              msg += " = " + QString::number(spb->value());
              
              if( digi[digiK]->WriteValue(para, std::to_string(spb->value()), index)){
                SendLogMsg(msg + "|OK.");
                spb->setStyleSheet("");
              }else{
                SendLogMsg(msg + "|Fail.");
                spb->setStyleSheet("color:red;");
              }
            }
          }
        }
      }else{
        QString msg;
        msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[digiID]->GetSerialNumber());
        msg += ",CH:" + QString::number(chID) + "(" + detType[typeID] + ")";
        msg += " = " + QString::number(spb->value());
        if( digi[digiID]->WriteValue(para, std::to_string(spb->value()), chID)){
          SendLogMsg(msg + "|OK.");
          spb->setStyleSheet("");
        }else{
          SendLogMsg(msg + "|Fail.");
          spb->setStyleSheet("color:red;");
        }
      }
      UpdatePanelFromMemory();
      UpdateOtherPanels();
    });
    
    ///===================== for the OnOff CheckBox
    connect(chkOnOff[SettingID][digiID][chID], &QCheckBox::stateChanged, this, [=](int state){
      if( !enableSignalSlot ) return; 

      if( chkAll[detGroup][SettingID]->isChecked() ){

        for(int k = 0; k < detIDList.size() ; k++){
          if( detGroup == detIDList[k][0] ){

            for( int h = ChStartIndex; h < detIDList[k].size() ; h++){
              if( h != chIndex + ChStartIndex) continue;
              int digiK = (detIDList[k][h] >> 8);
              if( digiK >= nDigi ) continue;
              int index = (detIDList[k][h] & 0xFF);
              QString msg;
              msg = QString::fromStdString(PHA::CH::ChannelEnable.GetPara()) + "|DIG:"+ QString::number(digi[digiK]->GetSerialNumber());
              msg += ",CH:" + QString::number(index) + "(" + detType[typeID] + ")";
              msg += ( state ? " = True" : " = False");
              
              if( digi[digiK]->WriteValue(PHA::CH::ChannelEnable, state ? "True" : "False", index)){
                SendLogMsg(msg + "|OK.");
                enableSignalSlot = false;

                leDisplay[detGroup][digiK][index]->setEnabled(state);
                sbSetting[detGroup][digiK][index]->setEnabled(state);
                chkOnOff [detGroup][digiK][index]->setChecked(state);
                enableSignalSlot = true;
              }else{
                SendLogMsg(msg + "|Fail.");
              }
            }
          }
        }

      }else{
        QString msg;
        msg = QString::fromStdString(PHA::CH::ChannelEnable.GetPara()) + "|DIG:"+ QString::number(digi[digiID]->GetSerialNumber());

        msg += ",CH:" + QString::number(chID) + "(" + detType[typeID] + ")";
        msg += ( state ? " = True" : " = False");

        qDebug() << msg;
        
        if( digi[digiID]->WriteValue(PHA::CH::ChannelEnable, state ? "True" : "False", chID)){
          SendLogMsg(msg + "|OK.");
          enableSignalSlot = false;
          leDisplay[detGroup][digiID][chID]->setEnabled(state);
          sbSetting[detGroup][digiID][chID]->setEnabled(state);
          chkOnOff [detGroup][digiID][chID]->setChecked(state);
          enableSignalSlot = true;
        }else{
          SendLogMsg(msg + "|Fail.");
        }
      }

      UpdatePanelFromMemory();
      UpdateOtherPanels();
    });

    layout0->setColumnStretch(0, 1);
    layout0->setColumnStretch(1, 1);
    layout0->setColumnStretch(2, 5);

  }

  //@======================================== Trigger Combox
  //=================== The Trigger depnds on 5 settings (at least)
  //   EventTriggerSource, WaveTriggerSource, CoincidentMask, AntiCoincidentMask
  // 1,  EventTriggerSource has 8 settings, ITLA, ITLB, GlobalTriggerSource, TRGIN, SWTrigger, ChSelfTrigger, Ch64Trigger, Disabled 
  // 2,  WaveTriggerSource  has 8 settings, always set to be equal EventTriggerSource
  // 3,  CoincidentMak      has 6 Settings, Disabled, Ch64Trigger, TRGIN, GloableTriggerSource, ITLA, ITLB
  // 4,  AntiCoincidentMask has 6 Settings, always disabled
  // 5,  ChannelTriggerMask is a 64-bit
  // 6,  CoincidenceLengthT in ns, set to be 100 ns. 

  if( SettingItems[SettingID].GetPara() == PHA::CH::TriggerThreshold.GetPara()){
    cbTrigger[detGroup][detID] = new RComboBox(this); 
    cbTrigger[detGroup][detID]->addItem("Self Trigger", "ChSelfTrigger");    /// no coincident
    cbTrigger[detGroup][detID]->addItem("Trigger e", 0x1); // Self-trigger and coincient Ch64Trigger
    cbTrigger[detGroup][detID]->addItem("Ext. Trigger", "TRGIN");  // with coincident with TRGIN.
    cbTrigger[detGroup][detID]->addItem("Disabled", "Disabled");  // no Trigger, no coincident, basically channel still alive, but no recording
    cbTrigger[detGroup][detID]->addItem("Others", -999); // other settings
    
    layout0->addWidget(cbTrigger[detGroup][detID], 2*nChInGroupBox, 0, 1, 3);

    if( isDisableDetector ) cbTrigger[detGroup][detID]->setEnabled(false);
  
    //*========== connection
    connect(cbTrigger[detGroup][detID], &RComboBox::currentIndexChanged, this , [=](int index){
      if( !enableSignalSlot) return;

      if( chkAll[detGroup][SettingID]->isChecked() ){


        //TODO if( detIDList[k][1] >= detMaxID[0] || 0 > detIDList[k][1]) continue;  //! only for array

        for( int gg = 0; gg < nDetinGroup[detGroup]; gg++){
          //TODO -==== if( gg >= ) 
          if( gg == detID ) continue;
          cbTrigger[detGroup][gg]->setCurrentIndex(index);
        }
      }

      ///----------- single
      for( int i = ChStartIndex ; i < detIDArray.size(); i++){

        int digiID = (detIDArray[i] >> 8 );
        if( digiID >= nDigi) continue;
        int chID = (detIDArray[i] & 0xFF);

        if( digi[digiID]->IsDummy() || !digi[digiID]->IsConnected() ) continue;

        digi[digiID]->WriteValue(PHA::CH::AntiCoincidenceMask, "Disabled", chID);

        switch(index){
          case 0 : { /// Self Trigger
            digi[digiID]->WriteValue(PHA::CH::EventTriggerSource, "ChSelfTrigger", chID);
            digi[digiID]->WriteValue(PHA::CH::WaveTriggerSource, "ChSelfTrigger", chID);
            digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Disabled", chID);
          }; break;
          case 1 : { /// trigger by energy
            digi[digiID]->WriteValue(PHA::CH::EventTriggerSource, "ChSelfTrigger", chID);
            digi[digiID]->WriteValue(PHA::CH::WaveTriggerSource, "ChSelfTrigger", chID);

            if( i == ChStartIndex ){
              digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Disabled", chID);
            }else {
              digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Ch64Trigger", chID);
              digi[digiID]->WriteValue(PHA::CH::CoincidenceLength, "100");

              //Form the trigger bit
              unsigned long mask = 1ULL << (detIDArray[ChStartIndex] & 0xFF ); // trigger by energy
              QString maskStr = QString::number(mask);
              digi[digiID]->WriteValue(PHA::CH::ChannelsTriggerMask, maskStr.toStdString() , chID);
            }
          }; break;
          case 2 : { /// TRGIN, when the whole board is trigger by TRG-IN
            digi[digiID]->WriteValue(PHA::CH::EventTriggerSource, "TRGIN", chID);
            digi[digiID]->WriteValue(PHA::CH::WaveTriggerSource, "TRGIN", chID);
            digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "TRGIN", chID);
            
            digi[digiID]->WriteValue(PHA::CH::CoincidenceLength, "100");
          }; break;
          case 3 : { /// disbaled
            digi[digiID]->WriteValue(PHA::CH::EventTriggerSource, "Disabled", chID);
            digi[digiID]->WriteValue(PHA::CH::WaveTriggerSource, "Disabled", chID);
            digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Disabled", chID);
          }; break;
        }

        SendLogMsg("SOLARIS panel : Set Trigger = " + cbTrigger[detGroup][detID]->itemText(index) + "|Digi:" + QString::number(digi[digiID]->GetSerialNumber()) + ",Det:" + QString::number(detID));

      }
      UpdatePanelFromMemory();
      UpdateOtherPanels();

    });


  }
  layout->addWidget(groupBox[detGroup][SettingID][detID], row, col);
}

//^##############################################################
void SOLARISpanel::RefreshSettings(){
  for( int i = 0 ; i < nDigi; i++){
    if( digi[i]->IsDummy() || !digi[i]->IsConnected()){
      digi[i]->ReadAllSettings();
    }
  }
  UpdatePanelFromMemory();
}

void SOLARISpanel::UpdatePanelFromMemory(){

  if( !isVisible() ) return;

  enableSignalSlot = false;

  printf("SOLARISpanel::%s\n", __func__);

  //@===================== LineEdit and SpinBox
  for( int SettingID = 0; SettingID < (int) SettingItems.size() ; SettingID ++){
    for( int DigiID = 0; DigiID < (int) mapping.size(); DigiID ++){
      if( DigiID >= nDigi ) continue;;
      for( int chID = 0; chID < (int) mapping[DigiID].size(); chID++){
        if( mapping[DigiID][chID] < 0 ) continue;

        std::string haha = digi[DigiID]->GetSettingValue(SettingItems[SettingID], chID);
        sbSetting[SettingID][DigiID][chID]->setValue( atof(haha.c_str()));

        if( SettingItems[SettingID].GetPara() == PHA::CH::TriggerThreshold.GetPara() ){
          std::string haha =  digi[DigiID]->GetSettingValue(PHA::CH::SelfTrgRate, chID);
          leDisplay[SettingID][DigiID][chID]->setText(QString::number(atof(haha.c_str()), 'f', 2) );
        }else{
          leDisplay[SettingID][DigiID][chID]->setText(QString::number(atof(haha.c_str()), 'f', 2) );
        }

        haha = digi[DigiID]->GetSettingValue(PHA::CH::ChannelEnable, chID);
        chkOnOff[SettingID][DigiID][chID]->setChecked( haha == "True" ? true : false);
        leDisplay[SettingID][DigiID][chID]->setEnabled(haha == "True" ? true : false);
        sbSetting[SettingID][DigiID][chID]->setEnabled(haha == "True" ? true : false);
        
        //printf("====== %d %d %d |%s|\n", SettingID, DigiID, chID, haha.c_str());
      }
    }
  }

  //@===================== Trigger
  for( int k = 0; k < detIDList.size() ; k++){
    if( detIDList[k][1] >= detMaxID[0] || 0 > detIDList[k][1]) continue;  //! only for array
    
    if( detIDList[k][0] != 0 ) continue;

    //if( detIDList[k].size() <= 2) continue;
    std::vector<unsigned long> triggerMap;
    std::vector<std::string> coincidentMask;
    std::vector<std::string> antiCoincidentMask;
    std::vector<std::string> eventTriggerSource;
    std::vector<std::string> waveTriggerSource;

    for( int h = ChStartIndex; h < detIDList[k].size(); h++){
      int chIndex = h - ChStartIndex;
      int digiID = detIDList[k][chIndex] >> 8;
      if( digiID >= nDigi) continue;

      int chID = (detIDList[k][h] & 0xFF);
      triggerMap.push_back(Utility::TenBase(digi[digiID]->GetSettingValue(PHA::CH::ChannelsTriggerMask, chID)));
      coincidentMask.push_back(digi[digiID]->GetSettingValue(PHA::CH::CoincidenceMask, chID));
      antiCoincidentMask.push_back(digi[digiID]->GetSettingValue(PHA::CH::AntiCoincidenceMask, chID));
      eventTriggerSource.push_back(digi[digiID]->GetSettingValue(PHA::CH::EventTriggerSource, chID));
      waveTriggerSource.push_back(digi[digiID]->GetSettingValue(PHA::CH::WaveTriggerSource, chID));
    }

    //====== only acceptable condition is eventTriggerSource are all ChSelfTrigger
    //       and coincidentMask for e, xf, xn, are at least one for Ch64Trigger
    //       and waveTriggerSource are all ChSelfTrigger


    int detTypeID = FindDetTypeID(detIDList[k][1]);

    //====== a stupid way
    // triggerSource : Other = 0x0, Disabled = 0x1, ChSelfTrigger = 0x2, TRGIN = 0x3
    // CoinMask      : Other = 0x0, Disbaled = 0x1, Ch64Trigger = 0x2, TRGIN = 0x3
    // for example, SelfTrigger should be,  0x2211, triggerMask does not matter.
    //                                        |||+-- anti-coin
    //                                        ||+-- coin
    //                                        |+-- wave
    //                                        +-- event

    // comboxIndex 0) SelfTrigger 0x2211, any trigger mask
    // comboxIndex 2) TRGIN       0x3331, any trigger mask
    // comboxIndex 3) Disable     0x1111, any trigger mask
    // comboxIndex 1) Trigger E   {0x2211 - e , 0x2221 - xf, 0x2221 - xn }, 
    //                            triggerMask { 0, 0x1, 0x1}
    // comboxIndex 4) Other settings

    std::vector<unsigned int> stupidIndex;
    for( int i = 0 ; i < (int) triggerMap.size(); i++){
      unsigned int index = 0;
      if(antiCoincidentMask[i] == "Disabled") index += 0x1;
      if(antiCoincidentMask[i] == "Ch64Trigger") index += 0x2;
      if(antiCoincidentMask[i] == "TRGIN") index += 0x3;
      
      if(coincidentMask[i] == "Disabled") index += 0x10;
      if(coincidentMask[i] == "Ch64Trigger") index += 0x20;
      if(coincidentMask[i] == "TRGIN") index += 0x30;

      if(waveTriggerSource[i] == "Disabled") index += 0x100;
      if(waveTriggerSource[i] == "ChSelfTrigger") index += 0x200;
      if(waveTriggerSource[i] == "TRGIN") index += 0x300;

      if(eventTriggerSource[i] == "Disabled") index += 0x1000;
      if(eventTriggerSource[i] == "ChSelfTrigger") index += 0x2000;
      if(eventTriggerSource[i] == "TRGIN") index += 0x3000;

      stupidIndex.push_back(index);
    }

    int jaja[5] = {0}; // this store the count for each comboxIndex;
    for( int i = 0; i < (int) stupidIndex.size(); i++){
      //printf(" %d | 0x%s \n", i, QString::number(stupidIndex[i], 16).toUpper().toStdString().c_str());
      if( stupidIndex[i] == 0x2211 ) jaja[0] ++;
      if( stupidIndex[i] == 0x3331 ) jaja[2] ++;
      if( stupidIndex[i] == 0x1111 ) jaja[3] ++;
      if( i == 0 && stupidIndex[i] == 0x2211 ) jaja[1] ++;
      if( i  > 0 && stupidIndex[i] == 0x2221 ) jaja[1] ++;
    }

    //Search for jaja, see is there any one equal to total number of ch;
    int comboxIndex = 4;
    for( int i = 0; i < 5; i++){
      //printf("%d | %d \n", i, jaja[i]);
      if( jaja[i] == (int) stupidIndex.size() )comboxIndex = i;
    }

    //printf("comboxIndex : %d \n", comboxIndex);

    // if Trigger e, need to check the trigger mask;
    if( comboxIndex == 3){
      unsigned long ShouldBeMask = 1ULL << (detIDList[k][ChStartIndex] & 0xFF);
      for( int i = 1; i < (int) triggerMap.size(); i ++){
        //printf(" %d | %lu =? %lu \n", i, triggerMap[i], ShouldBeMask);
        if( triggerMap[i] != ShouldBeMask) comboxIndex = 4;
      }  
    }

    cbTrigger[detTypeID][detIDList[k][1]]->setCurrentIndex(comboxIndex);

  }

  //@===================== Coin. time
  std::vector<int> coinTime;
  
  for( int i = 0; i < detIDList.size(); i++){
    for( int j = ChStartIndex; j < detIDList[i].size(); j++){
      int digiID = detIDList[i][j] >> 8;
      int chID = (detIDList[i][j] & 0xFF);
      if( digiID >= nDigi ) continue;
      if( digi[digiID]->IsDummy() || !digi[digiID]->IsConnected() ) continue;
      coinTime.push_back( atoi(digi[digiID]->GetSettingValue(PHA::CH::CoincidenceLength, chID).c_str()));
    }
  }

  bool isSameCoinTime = true;
  for( int i = 1; i < (int) coinTime.size(); i++){
    if( coinTime[i] != coinTime[0]) {
      isSameCoinTime = false;
      break;
    }
  }

  if( isSameCoinTime ){
    sbCoinTime->setValue(coinTime[0]);
  }else{
    sbCoinTime->setValue(-1);
  }

  enableSignalSlot = true;

}

void SOLARISpanel::UpdateThreshold(){

  for( int SettingID = 0; SettingID < (int) SettingItems.size() ; SettingID ++){
    if( SettingItems[SettingID].GetPara() != PHA::CH::TriggerThreshold.GetPara() ) continue;

    for( int DigiID = 0; DigiID < (int) mapping.size(); DigiID ++){
      if( DigiID >= nDigi ) continue;;

      for( int chID = 0; chID < (int) mapping[DigiID].size(); chID++){

        if( mapping[DigiID][chID] < 0 ) continue;
        
        std::string haha =  digi[DigiID]->GetSettingValue(PHA::CH::SelfTrgRate, chID);
        leDisplay[SettingID][DigiID][chID]->setText(QString::fromStdString(haha));
        
        ///printf("====== %d %d %d |%s|\n", SettingID, DigiID, chID, haha.c_str());
      }
    }
  }
}

//^#########################################
void SOLARISpanel::SaveSettings(){

  for(int i = 0; i < nDigi; i++){

    //*------ search for settings_XXXX.dat

    //Check path exist
    QDir dir(digiSettingPath);
    if( !dir.exists() ) dir.mkpath(".");

    QString settingFile = digiSettingPath + "/setting_" + QString::number(digi[i]->GetSerialNumber()) + "_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".dat";


    int flag = digi[i]->SaveSettingsToFile(settingFile.toStdString().c_str());
  
    switch (flag) {
      case 1 : {
        SendLogMsg("Saved setting file <b>" +  settingFile + "</b>.");
      }; break;
      case 0 : {
        SendLogMsg("<font style=\"color:red;\"> Fail to write setting file. <b> " + settingFile + " </b></font>");
      }; break;
      case -1 : {
        SendLogMsg("<font style=\"color:red;\"> Fail to save setting file <b> " + settingFile + " </b>, same settings are empty.</font>");
      }; break;
    };

  }
}

void SOLARISpanel::LoadSettings(){
  for(int i = 0; i < nDigi; i++){

    //*------ search for settings_XXXX.dat
    QString settingFile = digiSettingPath + "/settings_" + QString::number(digi[i]->GetSerialNumber()) + ".dat";
    if( digi[i]->LoadSettingsFromFile( settingFile.toStdString().c_str() ) ){
      SendLogMsg("Found setting file <b>" + settingFile + "</b> and loading. please wait.");
      digi[i]->SetSettingFileName(settingFile.toStdString());
      SendLogMsg("done settings.");
    }else{
      SendLogMsg("<font style=\"color: red;\">Unable to found setting file <b>" + settingFile + "</b>. </font>");
      digi[i]->SetSettingFileName("");
    }
    
  }

}