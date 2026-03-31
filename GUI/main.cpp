#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QFile>

int main(int argc, char *argv[]){

    printf("######### Starting SOLARIS DAQ....\n");

    QApplication a(argc, argv);

    // Read broker mode hint from programSettings.txt (line 11) for DAQ lock decision.
    // The actual broker auto-detection happens later in LoadProgramSettings via ping.
    bool brokerHint = false;
    {
      QFile settingsFile("programSettings.txt");
      if (settingsFile.open(QIODevice::Text | QIODevice::ReadOnly)) {
        QTextStream in(&settingsFile);
        for (int i = 0; !in.atEnd(); i++) {
          QString line = in.readLine();
          if (i == 11) { brokerHint = (line == "1"); break; }
        }
        settingsFile.close();
      }
    }

    // In broker mode, skip DAQ lock — multiple GUIs can connect
    bool isLock = false;
    int pid = 0;

    if (!brokerHint) {
      QFile lockFile(DAQLockFile);
      if( lockFile.open(QIODevice::Text | QIODevice::ReadOnly) ){
          QTextStream in(&lockFile);
          QString line = in.readLine();
          isLock = line.toInt();
          lockFile.close();
      }
      QFile pidFile(PIDFile);
      if( pidFile.open(QIODevice::Text | QIODevice::ReadOnly)){
          QTextStream in(&pidFile);
          QString line = in.readLine();
          pid = line.toInt();
          pidFile.close();
      }
    }

    if( isLock ) {

        qDebug() << "The DAQ program is already opened. PID is " + QString::number(pid) + ", and delete the " + DAQLockFile ;

        QMessageBox msgBox;
        msgBox.setWindowTitle("Oopss....");
        msgBox.setText("The DAQ program is already opened, or crashed perviously. \nPID is " + QString::number(pid) + "\n You can kill the procee by \"kill -9 <pid>\" and delete the " + DAQLockFile + "\n or click the \"Kill\" button");
        msgBox.setIcon(QMessageBox::Information);

        QPushButton * kill = msgBox.addButton("Kill and Open New", QMessageBox::AcceptRole);

        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);

        msgBox.exec();

        if(msgBox.clickedButton() == kill){
            remove(DAQLockFile);
            QProcess::execute("kill", QStringList() << "-9" << QString::number(pid));
        }else{
            return 0;
        }
    }

    if (!brokerHint) {
      QFile lockFile2(DAQLockFile);
      lockFile2.open(QIODevice::Text | QIODevice::WriteOnly);
      lockFile2.write( "1" );
      lockFile2.close();

      QFile pidFile2(PIDFile);
      pidFile2.open(QIODevice::Text | QIODevice::WriteOnly);
      pidFile2.write(  QString::number(QCoreApplication::applicationPid() ).toStdString().c_str() );
      pidFile2.close();
    }

    printf("######### Open Main Window...\n");
    MainWindow w;
    w.show();
    return a.exec();

}
