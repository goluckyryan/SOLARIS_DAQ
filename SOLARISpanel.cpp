#include "SOLARISpanel.h"

#include <QFile>
#include <QLabel>
#include <QSet>
#include <QList>
#include <QFrame>

#define NCOL 10 // number of column

std::vector<Reg> SettingItems = {PHA::CH::TriggerThreshold, PHA::CH::DC_Offset};
const std::vector<QString> arrayLabel = {"e", "xf", "xn"};

SOLARISpanel::SOLARISpanel(Digitizer2Gen **digi, unsigned short nDigi, 
                          std::vector<std::vector<int>> mapping, 
                          QStringList detType, 
                          std::vector<int> detMaxID, 
                          QWidget *parent) : QWidget(parent){

  setWindowTitle("SOLARIS Settings");
  setGeometry(0, 0, 1400, 800);

  printf("%s\n", __func__);

  this->digi = digi;
  this->nDigi = nDigi;
  this->mapping = mapping;
  this->detType = detType;
  this->detMaxID = detMaxID;

  nDigiMapping = mapping.size();
  for( int i = 0; i < nDigiMapping; i++) {
    int count = 0;
    for( int j = 0; j < (int )mapping[i].size(); j++) if( mapping[i][j] >= 0) count ++;
    nChMapping.push_back(count);
  }

  //Check number of detector type; Array 0-199, Recoil 200-299, other 300-
  int nDetType = detType.size();
  for( int k = 0 ; k < nDetType; k++) nDet.push_back(0);

  QList<QList<int>> detIDListTemp; // store { detID,  (Digi << 8 ) + ch}, making mapping into 1-D array to be consolidated

  printf("################################# \n");
  for( int i = 0; i < nDigiMapping ; i++){
    for( int j = 0; j < (int) mapping[i].size(); j++ ){
      printf("%3d,", mapping[i][j]);
      QList<int> haha ;
      haha << mapping[i][j] << ((i << 8 ) + j);
      if( mapping[i][j] >= 0 ) detIDListTemp <<  haha;
      if( j % 16 == 15 ) printf("\n");
    }
    printf("------------------ \n");
  }

  //----- consolidate detIDList;
  detIDList.clear();
  bool repeated = false;
  for( int i = 0; i < detIDListTemp.size(); i++ ){
    repeated = false;
    if( detIDList.size() == 0 ){
      detIDList << detIDListTemp[i];

      for( int k = 0 ; k < nDetType; k++){
        int lowID = (k == 0 ? 0 : detMaxID[k-1]);     
        if( lowID <= detIDListTemp[i][0] && detIDListTemp[i][0] < detMaxID[k] ) nDet[k] ++ ;
      }
      continue;
    }
    for( int j = 0; j < detIDList.size() ; j++){
      if( detIDList[j][0] == detIDListTemp[i][0] ) {
        repeated = true;
        detIDList[j] << detIDListTemp[i][1];
        break;
      }
    }
    if( !repeated ) {
      detIDList << detIDListTemp[i];
      for( int k = 0 ; k < nDetType; k++){
        int lowID = (k == 0 ? 0 : detMaxID[k-1]);     
        if( lowID <= detIDListTemp[i][0] && detIDListTemp[i][0] < detMaxID[k] ) nDet[k] ++ ;
      }
    }
  }

  //---- sort detIDList;
  std::sort(detIDList.begin(), detIDList.end(), [](const QList<int>& a, const QList<int>& b) {
    return a.at(0) < b.at(0);
  });



  //------------- display detector summary
  //qDebug() << detIDList;
  printf("---------- num. of det. Type : %d\n", nDetType);
  for( int i =0; i < nDetType; i ++ ) {
    printf(" Type-%d (%6s) : %3d det.  (%3d - %3d)\n", i, detType[i].toStdString().c_str(), nDet[i], (i==0 ? 0 : detMaxID[i-1]), detMaxID[i]-1);
  }

  //------------ create Widgets
  leDisplay = new QLineEdit ***[(int) SettingItems.size()];
  sbSetting = new RSpinBox  ***[(int) SettingItems.size()];
  chkOnOff  = new QCheckBox ***[(int) SettingItems.size()];
  for( int k = 0; k < (int) SettingItems.size() ; k ++){
    leDisplay[k] = new QLineEdit **[nDigiMapping];
    sbSetting[k] = new RSpinBox  **[nDigiMapping];
    chkOnOff[k]  = new QCheckBox **[nDigiMapping];
    for( int i = 0; i < nDigiMapping; i++){
      leDisplay[k][i] = new QLineEdit *[(int) mapping[i].size()];
      sbSetting[k][i] = new RSpinBox  *[(int) mapping[i].size()];
      chkOnOff[k][i]  = new QCheckBox *[(int) mapping[i].size()];
      for( int j = 0 ; j < (int) mapping[i].size() ; j++){
        leDisplay[k][i][j] = nullptr;
        sbSetting[k][i][j] = nullptr;
        chkOnOff[k][i][j] = nullptr;
      }
    }
  }

  cbTrigger = new RComboBox ** [nDetType];
  for(int i = 0; i < nDetType; i++){
    cbTrigger[i] = new RComboBox *[nDet[i]]; /// nDet[0] store the number of det. for the Array 
    for( int j = 0; j < nDet[i]; j ++){
      cbTrigger[i][j] = nullptr;
    }
  }
  //---------- Set Panel
  QVBoxLayout * mainLayout = new QVBoxLayout(this); this->setLayout(mainLayout);
  QTabWidget * tabWidget = new QTabWidget(this); mainLayout->addWidget(tabWidget);

  for( int detTypeID = 0; detTypeID < nDetType; detTypeID ++ ){

    QTabWidget * tab2 = new QTabWidget(tabWidget);
    tabWidget->addTab(tab2, detType[detTypeID]);

    for( int SettingID = 0; SettingID < (int) SettingItems.size(); SettingID ++){

      QScrollArea * scrollArea = new QScrollArea(this);
      scrollArea->setWidgetResizable(true);
      scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      tab2->addTab(scrollArea,  QString::fromStdString(SettingItems[SettingID]));

      QWidget * tab = new QWidget(tab2);
      scrollArea->setWidget(tab);
      
      QGridLayout * layout = new QGridLayout(tab);
      layout->setAlignment(Qt::AlignLeft|Qt::AlignTop);
      layout->setSpacing(0);

      //TODO======= check all
      chkAll = new QCheckBox("Set for all", tab);
      layout->addWidget(chkAll, 0, 0);

      if( SettingItems[SettingID].GetPara() == PHA::CH::TriggerThreshold.GetPara() ){
        chkAlle = new QCheckBox("Set for all e", tab);
        layout->addWidget(chkAlle, 0, 1);

        chkAllxf = new QCheckBox("Set for all xf", tab);
        layout->addWidget(chkAllxf, 0, 2);

        chkAllxn = new QCheckBox("Set for all xn", tab);
        layout->addWidget(chkAllxn, 0, 3);

      }

      QFrame *line = new QFrame(tab);
      line->setFrameShape(QFrame::HLine);
      line->setFrameShadow(QFrame::Sunken);
      line->setFixedHeight(10);
      layout->addWidget(line, 1, 0, 1, NCOL);


      //range of detID 
      int lowID = (detTypeID == 0 ? 0 : detMaxID[detTypeID-1]);

      for(int i = 0; i < detIDList.size(); i++){
        if( detIDList[i][0] >= detMaxID[detTypeID] || lowID > detIDList[i][0] ) continue;
        CreateDetGroup(SettingID, detIDList[i], layout, i/NCOL +  2, i%NCOL);
      }

    }
  }

  //TODO ========== when digiSettingPanel or Scope change setting, reflect in solarisPanel

  //TODO ========== when tab change, update panel

}

SOLARISpanel::~SOLARISpanel(){

  printf("%s\n", __func__);

  for( int k = 0; k < (int) SettingItems.size() ; k ++){
    for( int i = 0; i < nDigiMapping; i++){
      delete [] leDisplay[k][i] ;
      delete [] sbSetting[k][i] ;
      delete [] chkOnOff[k][i] ;
    }
    delete [] leDisplay[k];
    delete [] sbSetting[k];
    delete [] chkOnOff[k] ;
  }

  delete [] leDisplay;
  delete [] sbSetting;
  delete [] chkOnOff;

  for( int k = 0; k < (int) detType.size() ; k++){
    delete [] cbTrigger[k];
  }
  delete [] cbTrigger;

  printf("end of %s\n", __func__);

}

void SOLARISpanel::CreateDetGroup(int SettingID, QList<int> detID, QGridLayout * &layout, int row, int col){

  QGroupBox * groupbox = new QGroupBox("Det-" + QString::number(detID[0]), this);
  groupbox->setFixedWidth(130);
  groupbox->setAlignment(Qt::AlignCenter);
  QGridLayout * layout0 = new QGridLayout(groupbox);
  layout0->setSpacing(0);
  layout0->setAlignment(Qt::AlignLeft);

  for( int i = 1; i < (int) detID.size(); i ++){

    QLabel * lb  = new QLabel(arrayLabel[i-1], this);  
    layout0->addWidget(lb, 2*i, 0, 2, 1);

    int digiID = (detID[i] >> 8 );
    int chID = (detID[i] & 0xFF);

    chkOnOff[SettingID][digiID][chID] = new QCheckBox(this);
    layout0->addWidget(chkOnOff[SettingID][digiID][chID], 2*i, 1, 2, 1);

    leDisplay[SettingID][digiID][chID] = new QLineEdit(this);
    leDisplay[SettingID][digiID][chID]->setFixedWidth(70);
    layout0->addWidget(leDisplay[SettingID][digiID][chID], 2*i, 2);

    sbSetting[SettingID][digiID][chID] = new RSpinBox(this);
    if( digiID < nDigi ) sbSetting[SettingID][digiID][chID]->setToolTip( "Digi-" + QString::number(digi[digiID]->GetSerialNumber()) + ", Ch-" + QString::number(chID));
    sbSetting[SettingID][digiID][chID]->setToolTipDuration(-1);
    sbSetting[SettingID][digiID][chID]->setFixedWidth(70);

    sbSetting[SettingID][digiID][chID]->setMinimum(atoi(SettingItems[SettingID].GetAnswers()[0].first.c_str()));
    sbSetting[SettingID][digiID][chID]->setMaximum(atoi(SettingItems[SettingID].GetAnswers()[1].first.c_str()));
    sbSetting[SettingID][digiID][chID]->setSingleStep(atoi(SettingItems[SettingID].GetAnswers()[2].first.c_str()));

    layout0->addWidget(sbSetting[SettingID][digiID][chID], 2*i+1, 2);

    if( digiID >= nDigi || chID >= digi[digiID]->GetNChannels() ) {
      leDisplay[SettingID][digiID][chID]->setEnabled(false);
      sbSetting[SettingID][digiID][chID]->setEnabled(false);    
      chkOnOff[SettingID][digiID][chID]->setEnabled(false);    
    }

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
      QString msg;
      msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[digiID]->GetSerialNumber());
      if( para.GetType() == TYPE::CH ) msg += ",CH:" + QString::number(chID);
      msg += " = " + QString::number(spb->value());
      if( digi[digiID]->WriteValue(para, std::to_string(spb->value()), chID)){
        SendLogMsg(msg + "|OK.");
        spb->setStyleSheet("");
      }else{
        SendLogMsg(msg + "|Fail.");
        spb->setStyleSheet("color:red;");
      }
    });
    

    connect(chkOnOff[SettingID][digiID][chID], &QCheckBox::stateChanged, this, [=](){
      if( !enableSignalSlot ) return; 

      bool haha = chkOnOff[SettingID][digiID][chID]->isChecked();

      if( haha ) {
        digi[digiID]->WriteValue(PHA::CH::ChannelEnable, "True", chID);
      }else{
        digi[digiID]->WriteValue(PHA::CH::ChannelEnable, "False", chID);
      } 
      leDisplay[SettingID][digiID][chID]->setEnabled(haha);
      sbSetting[SettingID][digiID][chID]->setEnabled(haha);
    });

    layout0->setColumnStretch(0, 1);
    layout0->setColumnStretch(1, 1);
    layout0->setColumnStretch(2, 5);

  }

  int detTypeID = 0;
  for( int i = 0; i < (int) detType.size(); i++){
    int lowID = (i == 0) ? 0 : detMaxID[i-1];
    if( lowID <= detID[0] && detID[0] < detMaxID[i]) {
      detTypeID = i;
      break;
    }
  }

  if( SettingItems[SettingID].GetPara() == PHA::CH::TriggerThreshold.GetPara()){
    cbTrigger[detTypeID][detID[0]] = new RComboBox(this);
    cbTrigger[detTypeID][detID[0]]->addItem("Non-Trigger", 0x0);
    cbTrigger[detTypeID][detID[0]]->addItem("Self Trigger",  -1);
    cbTrigger[detTypeID][detID[0]]->addItem("Trigger all", 0x7);
    cbTrigger[detTypeID][detID[0]]->addItem("Trigger (e)", 0x1);
    cbTrigger[detTypeID][detID[0]]->addItem("Trigger (xf)", 0x2);
    cbTrigger[detTypeID][detID[0]]->addItem("Trigger (xn)", 0x4);
    cbTrigger[detTypeID][detID[0]]->addItem("Trigger 011", 0x3);
    cbTrigger[detTypeID][detID[0]]->addItem("Trigger 110", 0x6);
    cbTrigger[detTypeID][detID[0]]->addItem("Trigger 101", 0x5);
    cbTrigger[detTypeID][detID[0]]->addItem("Oops....", -999);
    layout0->addWidget(cbTrigger[detTypeID][detID[0]], 8, 0, 1, 3);
  }

  layout->addWidget(groupbox, row, col);

}


void SOLARISpanel::UpdatePanel(){
  enableSignalSlot = false;

  for( int SettingID = 0; SettingID < (int) SettingItems.size() ; SettingID ++){
    for( int DigiID = 0; DigiID < (int) mapping.size(); DigiID ++){
      if( DigiID >= nDigi ) continue;;

      for( int chID = 0; chID < (int) mapping[DigiID].size(); chID++){

        if( mapping[DigiID][chID] < 0 ) continue;

        std::string haha = digi[DigiID]->GetSettingValue(SettingItems[SettingID], chID);
        sbSetting[SettingID][DigiID][chID]->setValue( atof(haha.c_str()));

        if( SettingItems[SettingID].GetPara() == PHA::CH::TriggerThreshold.GetPara() ){
          std::string haha =  digi[DigiID]->GetSettingValue(PHA::CH::SelfTrgRate, chID);
          leDisplay[SettingID][DigiID][chID]->setText(QString::fromStdString(haha));
        }else{
          leDisplay[SettingID][DigiID][chID]->setText(QString::fromStdString(haha));
        }

        haha = digi[DigiID]->GetSettingValue(PHA::CH::ChannelEnable, chID);
        chkOnOff[SettingID][DigiID][chID]->setChecked( haha == "True" ? true : false);

        ///printf("====== %d %d %d |%s|\n", SettingID, DigiID, chID, haha.c_str());
      }
    }
  }

  for( int k = 0; k < detIDList.size() ; k++){
    if( detIDList[k][0] >= detMaxID[0] || 0 > detIDList[k][0]) continue;
    
    //if( detIDList[k].size() <= 2) continue;
    std::vector<unsigned long> triggerMap;
    std::vector<std::string> coincidentMask;
    std::vector<std::string> eventTriggerSource;

    for( int h = 1; h < detIDList[k].size(); h++){
      int digiID = detIDList[k][h] >> 8;
      int chID = (detIDList[k][h] & 0xFF);
      triggerMap.push_back(std::stoul(digi[digiID]->GetSettingValue(PHA::CH::ChannelsTriggerMask, chID).c_str()));
      coincidentMask.push_back(digi[digiID]->GetSettingValue(PHA::CH::CoincidenceMask, chID));
      eventTriggerSource.push_back(digi[digiID]->GetSettingValue(PHA::CH::EventTriggerSource, chID));
    }

    //====== only acceptable condition is eventTriggerSource are all ChSelfTrigger
    //       and coincidentMask for e, xf, xn, are at least one for Ch64Trigger

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
