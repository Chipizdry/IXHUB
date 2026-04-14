


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QQuickWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class RelayDevice;
struct ChartData;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void onPowerButtonToggled(bool state);
    void updateSpeed(int value);
    void updatePower(int value);
    void updateTorque(int value);

private slots:
    void initializeChart();
    void updateIndicators();
    void updateChart();

private:
    Ui::MainWindow *ui;
    QQuickWidget *powerWidget;
    QQuickWidget *speedWidget;
    QQuickWidget *torqueWidget;
    QQuickWidget *powerButtonWidget;
    QQuickWidget *m_chartWidget;
    RelayDevice *m_relayDevice;
    ChartData *m_chartData;
};

#endif // MAINWINDOW_H


