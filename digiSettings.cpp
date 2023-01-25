#include "digiSettings.h"

#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QGridLayout>
#include <QScrollArea>
#include <QTabWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QDebug>

DigiSettings::DigiSettings(Digitizer2Gen * digi, unsigned short nDigi, QWidget * parent) : QWidget(parent){

  qDebug() << "DigiSettings constructor";

  setWindowTitle("Digitizers Settings");
  setGeometry(200, 50, 1000, 1000);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);


  this->digi = digi;
  this->nDigi = nDigi;


  std::vector<std::vector<std::string>> info = {{"Serial Num : ", "/par/SerialNum"},
                                                {"IP : ", "/par/IPAddress"},
                                                {"Model Name : ", "/par/ModelName"},
                                                {"FPGA firmware version : ", "/par/FPGA_FwVer"},
                                                {"DPP Type : ", "/par/FwType"},
                                                {"CUP version : ", "/par/cupver"},
                                                {"ADC bits : ", "/par/ADC_Nbit"},
                                                {"ADC rate [Msps] : ", "/par/ADC_SamplRate"},
                                                {"Number of Channel : ", "/par/NumCh"}
                                               };

  QVBoxLayout * mainLayout = new QVBoxLayout(this);
  this->setLayout(mainLayout);
  QTabWidget * tabWidget = new QTabWidget(this);
  mainLayout->addWidget(tabWidget);

  //============ Tab for each digitizer
  for(unsigned short i = 0; i < nDigi; i++){
  

    QWidget * tab = new QWidget(tabWidget);
    
    QScrollArea * scrollArea = new QScrollArea;
    scrollArea->setWidget(tab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    tabWidget->addTab(scrollArea, "Digi-" + QString::number(digi->GetSerialNumber()));

    QGridLayout *tabLayout = new QGridLayout(tab);
    tab->setLayout(tabLayout);

    //-------- Group of Digitizer Info
    QGroupBox * infoBox = new QGroupBox("Board Info", tab);
    QGridLayout * infoLayout = new QGridLayout(infoBox);
    infoBox->setLayout(infoLayout);

    for( unsigned short j = 0; j < (unsigned short) info.size(); j++){
      QLabel * lab = new QLabel(QString::fromStdString(info[j][0]), tab);
      lab->setAlignment(Qt::AlignRight);
      QLineEdit * txt = new QLineEdit(tab);
      txt->setReadOnly(true);
      txt->setText(QString::fromStdString(digi->ReadValue(info[j][1].c_str())));
      //txt->setStyleSheet("color: black;");
      infoLayout->addWidget(lab, j, 0);
      infoLayout->addWidget(txt, j, 1);
    }
    
    tabLayout->addWidget(infoBox, 0, 0);

    //------- Group digitizer settings
    QGroupBox * digiBox = new QGroupBox("Board Settings", tab);
    QGridLayout * digiLayout = new QGridLayout(digiBox);
    digiBox->setLayout(digiLayout);

    tabLayout->addWidget(digiBox, 1, 0, 4, 1);


    //------- Group channel settings
    QGroupBox * chBox = new QGroupBox("Channel Settings", tab);
    QGridLayout * chLayout = new QGridLayout(chBox);
    chBox->setLayout(chLayout);

    for( unsigned short ch = 0; ch < digi->GetNChannels(); ch++){
      QLabel * labCh = new QLabel(QString::number(ch), tab);
      labCh->setAlignment(Qt::AlignRight);
      chLayout->addWidget(labCh, ch, 0);

      QCheckBox * cbCh = new QCheckBox(tab);
      std::string onOff = digi->ReadValue(("/ch/" + std::to_string(ch)+ "/par/ChEnable").c_str());
      //qDebug() << QString::fromStdString(std::to_string(ch)  +  ", " + onOff);
      if( onOff == "True"){
        cbCh->setChecked(true);
      }else{
        cbCh->setChecked(false);
      }

      chLayout->addWidget(cbCh, ch, 1);

    }

    tabLayout->addWidget(chBox, 0, 1, 5, 1);

  }


}

DigiSettings::~DigiSettings(){


}