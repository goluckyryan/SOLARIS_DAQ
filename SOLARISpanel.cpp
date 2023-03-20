#include "SOLARISpanel.h"

#include <QFile>
#include <QLabel>
#include <QSet>
#include <QList>

#define NCOL 20 // number of column

std::vector<Reg> SettingItems = {PHA::CH::TriggerThreshold, PHA::CH::DC_Offset};

const std::vector<QString> arrayLabel = {"e", "xf", "xn"};

SOLARISpanel::SOLARISpanel(Digitizer2Gen **digi, unsigned short nDigi, 
                          std::vector<std::vector<int>> mapping, 
                          QStringList detType, 
                          std::vector<int> detMaxID, 
                          QWidget *parent) : QWidget(parent){

  setWindowTitle("SOLARIS Settings");
  setGeometry(0, 0, 1850, 750);

  printf("%s\n", __func__);

  this->digi = digi;
  this->nDigi = nDigi;
  this->mapping = mapping;
  this->detType = detType;
  this->detMaxID = detMaxID;

  nDigiMapping = mapping.size();
  for( int i = 0; i < nDigiMapping; i++) nChMapping.push_back(mapping[i].size());

  //Check number of detector type; Array 0-199, Recoil 200-299, other 300-
  int nDetType = detType.size();

  int nDet[nDetType];
  for( int k = 0 ; k < nDetType; k++) nDet[k] = 0;

  QList<QList<int>> detIDListTemp; //to be consolidated

  printf("################################# \n");
  for( int i = 0; i < nDigiMapping ; i++){
    for( int j = 0; j < nChMapping[i]; j++ ){
      printf("%3d,", mapping[i][j]);
      QList<int> haha ;
      haha << i << mapping[i][j];
      if( mapping[i][j] >= 0 ) detIDListTemp <<  haha;
      for( int k = 0 ; k < nDetType; k++){
        int lowID = (k == 0 ? 0 : detMaxID[k-1]);     
        if( lowID <= mapping[i][j] && mapping[i][j] < detMaxID[k] ) nDet[k] ++ ;
      }
      if( j % 16 == 15 ) printf("\n");
    }
    printf("------------------ \n");
  }

  //----- consolidate detIDList;
  QList<QList<int>> detIDList;
  detIDList << detIDListTemp[0];
  bool repeated = false;
  for( int i = 0; i < detIDListTemp.size(); i++ ){
    repeated = false;
    for( int j = 0; j < detIDList.size() ; j++){
      if( detIDList[j] == detIDListTemp[i] ) {
        repeated = true;
        break;
      }
    }
    if( !repeated ) detIDList << detIDListTemp [i];
  }

  //------------ create Widgets
  leDisplay = new QLineEdit***[(int) SettingItems.size()];
  sbSetting = new RSpinBox***[(int) SettingItems.size()];
  for( int k = 0; k < (int) SettingItems.size() ; k ++){
    leDisplay[k] = new QLineEdit**[nDigiMapping];
    sbSetting[k] = new RSpinBox**[nDigiMapping];
    for( int i = 0; i < nDigiMapping; i++){
      sbSetting[k][i] = new RSpinBox*[(int) mapping[i].size()];
      leDisplay[k][i] = new QLineEdit*[(int) mapping[i].size()];
    }
  }

  //---------- Set Panel
  QVBoxLayout * mainLayout = new QVBoxLayout(this); this->setLayout(mainLayout);
  QTabWidget * tabWidget = new QTabWidget(this); mainLayout->addWidget(tabWidget);

  for( int detTypeID = 0; detTypeID < nDetType; detTypeID ++ ){

    QTabWidget * tab2 = new QTabWidget(tabWidget);
    tabWidget->addTab(tab2, detType[detTypeID]);

    for( int SettingID = 0; SettingID < (int) SettingItems.size(); SettingID ++){

      QWidget * tab = new QWidget(tab2);
      tab2->addTab(tab, QString::fromStdString(SettingItems[SettingID]));

      QGridLayout * layout = new QGridLayout(tab);
      layout->setAlignment(Qt::AlignLeft|Qt::AlignTop);
      layout->setSpacing(0);

      QCheckBox * chkAll = new QCheckBox("Set for all", tab);
      layout->addWidget(chkAll, 0, 0);

      //range of detID 
      int lowID = (detTypeID == 0 ? 0 : detMaxID[detTypeID-1]);

      for(int i = 0; i < detIDList.size(); i++){
        if( detIDList[i][1] >= detMaxID[detTypeID] || lowID > detIDList[i][1] ) continue;
        CreateSpinBoxGroup(SettingID, detIDList[i], layout, i/NCOL +  1, i%NCOL);
      }

    }
  }

}

SOLARISpanel::~SOLARISpanel(){

}

void SOLARISpanel::CreateSpinBoxGroup(int SettingID, QList<int> detID, QGridLayout * &layout, int row, int col){

  //find all chID = (iDigi << 8 + ch) for detID
  std::vector<int> chID;
  for( int i = 0; i < nDigiMapping ; i++){
    for( int j = 0; j < nChMapping[i]; j++ ){
      if( mapping[i][j] == detID[1] ) chID.push_back((i << 8)  + j);
    }
  }

  QGroupBox * groupbox = new QGroupBox("Det-" + QString::number(detID[1]), this);
  groupbox->setFixedWidth(100);
  QGridLayout * layout0 = new QGridLayout(groupbox);

  for( int i = 0; i < (int) chID.size(); i ++){

    QLabel * lb  = new QLabel(arrayLabel[i], this);  
    layout0->addWidget(lb, 2*i, 0, 2, 1);

    int digiID = (chID[i] >> 8);
    int ch = (chID[i] & 0xFF);

    leDisplay[SettingID][digiID][ch] = new QLineEdit(this);
    leDisplay[SettingID][digiID][ch]->setFixedWidth(50);
    layout0->addWidget(leDisplay[SettingID][digiID][ch], 2*i, 1);
  
    sbSetting[SettingID][digiID][ch] = new RSpinBox(this);
    if( digiID < nDigi ) sbSetting[SettingID][digiID][ch]->setToolTip( "Digi-" + QString::number(digi[digiID]->GetSerialNumber()) + ",Ch-" + QString::number(ch));
    sbSetting[SettingID][digiID][ch]->setToolTipDuration(-1);
    sbSetting[SettingID][digiID][ch]->setFixedWidth(50);
    layout0->addWidget(sbSetting[SettingID][digiID][ch], 2*i+1, 1);

    RSpinBox * spb = sbSetting[SettingID][digiID][ch];

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
      if( para.GetType() == TYPE::CH ) msg += ",CH:" + QString::number(ch);
      msg += " = " + QString::number(spb->value());
      if( digi[digiID]->WriteValue(para, std::to_string(spb->value()), ch)){
        SendLogMsg(msg + "|OK.");
        spb->setStyleSheet("");
      }else{
        SendLogMsg(msg + "|Fail.");
        spb->setStyleSheet("color:red;");
      }
    });

  }

  if( detID[0] >= nDigi || detID[1] >= digi[detID[0]]->GetNChannels() ) groupbox->setEnabled(false);

  layout->addWidget(groupbox, row, col);

}


void SOLARISpanel::UpdatePanel(){

  enableSignalSlot = false;
  
  for( int SettingID = 0; SettingID < (int) SettingItems.size() ; SettingID ++){
    for( int DigiID = 0; DigiID < (int) mapping.size(); DigiID ++){
      if( DigiID >= nDigi ) continue;;

      for( int chID = 0; chID < (int) mapping[DigiID].size(); chID++){

        if( chID >= digi[DigiID]->GetNChannels() ) continue;
        if( mapping[DigiID][chID] < 0 ) continue;

        std::string haha = digi[DigiID]->GetSettingValue(SettingItems[SettingID], chID);
        sbSetting[SettingID][DigiID][chID]->setValue( atof(haha.c_str()));

        if( SettingItems[SettingID].GetPara() == PHA::CH::TriggerThreshold.GetPara() ){
          std::string haha =  digi[DigiID]->GetSettingValue(PHA::CH::SelfTrgRate, chID);
          leDisplay[SettingID][DigiID][chID]->setText(QString::fromStdString(haha));
        }else{
          leDisplay[SettingID][DigiID][chID]->setText(QString::fromStdString(haha));
        }
        ///printf("====== %d %d %d |%s|\n", SettingID, DigiID, chID, haha.c_str());
      }
    }
  }

  enableSignalSlot = true;
}

void SOLARISpanel::UpdateThreshold(){

  for( int SettingID = 0; SettingID < (int) SettingItems.size() ; SettingID ++){
    if( SettingItems[SettingID].GetPara() != PHA::CH::TriggerThreshold.GetPara() ) continue;

    for( int DigiID = 0; DigiID < (int) mapping.size(); DigiID ++){
      if( DigiID >= nDigi ) continue;;

      for( int chID = 0; chID < (int) mapping[DigiID].size(); chID++){

        if( chID >= digi[DigiID]->GetNChannels() ) continue;
        if( mapping[DigiID][chID] < 0 ) continue;
        
        std::string haha =  digi[DigiID]->GetSettingValue(PHA::CH::SelfTrgRate, chID);
        leDisplay[SettingID][DigiID][chID]->setText(QString::fromStdString(haha));
        
        ///printf("====== %d %d %d |%s|\n", SettingID, DigiID, chID, haha.c_str());
      }
    }
  }

}
