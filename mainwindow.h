#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QMainWindow>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QThread>
#include <qdebug.h>
#include <QDateTime>
#include <QScrollBar>
#include <QPushButton>

#include "ClassDigitizer2Gen.h"

static Digitizer2Gen * digi = NULL;

class MainWindow : public QMainWindow{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    //void onThreadStarted(){ qDebug() << "kkkkkkkkkkk"; }
    //void onThreadFinished(){ qDebug() << "thread done"; }

    void bnOpenDigitizers_clicked();
    void bnCloseDigitizers_clicked();

signals :


private:
    QPushButton * bnProgramSettings;
    QPushButton * bnOpenDigitizers;
    QPushButton * bnCloseDigitizers;
    QPushButton * bnDigiSettings;

    QPushButton * bnStartACQ;
    QPushButton * bnStopACQ;

    QPlainTextEdit * logInfo;

    unsigned short nDigi;

    //QThread * StartRunThread;

    void LogMsg(QString msg);

};


#endif // MAINWINDOW_H
