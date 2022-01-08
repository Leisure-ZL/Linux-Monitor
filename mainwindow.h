#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qcustomplot.h"
#include <QTimer>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    int key =0 ;
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
private:
    int curTime,curTotal,curIdle,curTranTime,curRecvByte,curSendByte;
    QMap<int,int> procPreTimeMap;
    QTimer timer,procTimer;
    Ui::MainWindow *ui;
    void Init();
    int IsPid(const struct dirent *entry);
    void PaintGraph(QCustomPlot *customPlot);

private slots:
    void DispProc();
    void DispCpuStat();
    void DispMemStat();
    void DispDiskStat();
    void DispNetStat();
    void on_listWidget_itemClicked(QListWidgetItem *item);
    void on_pushButton_clicked();
    void on_tabWidget_currentChanged(int index);
    void on_poweroffButton_clicked();
    void on_restartButton_clicked();
};

#endif // MAINWINDOW_H
