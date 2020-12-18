#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    widgetInit();
    connect(timer,SIGNAL(timeout()),this,SLOT(searchSerialport()));
    connect(ui->openButton,SIGNAL(clicked()),this,SLOT(changeSerialState()));
    connect(ui->serialportBox,SIGNAL(activated(int)),this,SLOT(recordSerialChoice(int)));
    connect(serialport,SIGNAL(readyRead()),this,SLOT(readSerialport()));
    connect(collision,SIGNAL(stopSignal(quint8,quint8)),this,SLOT(emitStopSignal(quint8,quint8)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::widgetInit()
{
    serial_state=CLOSESTATE;
    serial_choice="";
    max_line_id=0;
    ui->openButton->setIcon(QPixmap(":/state/close.png"));
    ui->openButton->setText("打开串口");
    ui->widget->setInteractions(QCP::iSelectAxes|QCP::iSelectLegend|QCP::iSelectPlottables);
    timer->start(BASETIME);
}

void MainWindow::changeSerialState()
{
    if(serial_state==CLOSESTATE && openSerialport())
    {
        ui->openButton->setIcon(QPixmap(":/state/open.png"));
        ui->openButton->setText("关闭串口");
        serial_state=OPENSTATE;
    }
    else if(serial_state==OPENSTATE)
    {
        ui->openButton->setIcon(QPixmap(":/state/close.png"));
        ui->openButton->setText("打开串口");
        serial_state=CLOSESTATE;
        closeSerialport();
    }
}

void MainWindow::recordSerialChoice(int choice)
{
    serial_choice=ui->serialportBox->itemText(choice);
    if(serial_state==OPENSTATE)
    {
        closeSerialport();
        openSerialport();
    }
}

void MainWindow::searchSerialport()
{
    QList<QString> temp_list;
    if(QSerialPortInfo::availablePorts().size()!=0)
    {
        if(ui->serialportBox->itemText(0)=="")
        {
            ui->serialportBox->removeItem(0);
        }
        if(serialport_list.isEmpty())
        {
            foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
            {
                serialport_list.append(info.portName());
            }
            for(int i=0;i<serialport_list.size();i++)
            {
                ui->serialportBox->addItem(serialport_list[i]);
            }
        }
        else
        {
            foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
            {
                temp_list.append(info.portName());
            }
            if(temp_list!=serialport_list)
            {
                ui->serialportBox->clear();
                for(int i=0;i<temp_list.size();i++)
                {
                    ui->serialportBox->addItem(temp_list[i]);
                }
                serialport_list=temp_list;
            }
        }
        ui->serialportBox->setCurrentText(serial_choice);
    }
    else
    {
        ui->serialportBox->clear();
        ui->serialportBox->addItem("");
        serialport_list.clear();
    }
}

bool MainWindow::openSerialport()
{
    serialport->setPortName(ui->serialportBox->currentText());
    serialport->setBaudRate(ui->baudrateBox->currentText().toInt());
    serialport->setStopBits(QSerialPort::OneStop);
    serialport->setFlowControl(QSerialPort::NoFlowControl);
    serialport->setDataBits(QSerialPort::Data8);
    serialport->setParity(QSerialPort::NoParity);
    QSerialPortInfo serial_info(ui->serialportBox->currentText());
    if(serial_info.isBusy()==true)
    {
        QString dlgTitle="错误";
        QString strInfo="打开串口失败!串口已被占用!";
        QMessageBox::critical(this,dlgTitle,strInfo);
        serial_state=OPENSTATE;
        changeSerialState();
        return false;
    }
    serialport->open(QIODevice::ReadWrite);
    return true;
}

void MainWindow::closeSerialport()
{
    serialport->close();
}

void MainWindow::writeSerialport(quint8 *data,int count)
{
    QByteArray buffer;
    buffer.resize(count);
    memcpy(buffer.data(),data,count);
    serialport->write(buffer);
}

void MainWindow::readSerialport()
{
    quint8 *rec_buffer=(quint8*)serialport->readAll().data();
    int sum=0;
    quint8 line_id;
    for(int i=0;i<=6;i++)
    {
        sum+=rec_buffer[i];
    }
    if(sum!=(rec_buffer[7]<<8|rec_buffer[8]))
    {
        return;
    }
    int x=(rec_buffer[1]==0x00?rec_buffer[2]<<8|rec_buffer[3]:-(rec_buffer[2]<<8|rec_buffer[3]));
    int y=(rec_buffer[4]==0x00?rec_buffer[5]<<8|rec_buffer[6]:-(rec_buffer[5]<<8|rec_buffer[6]));
    if(rec_buffer[0]==0x00)
    {
        ui->widget->xAxis->setRange(0,x);//设置x轴长度
        ui->widget->yAxis->setRange(0,y);//设置y轴长度
        ui->widget->replot();
        collision->createMap(x,y);
    }
    else
    {
        if(id_list.indexOf(rec_buffer[0])==-1)
        {
            line_id=addLine(rec_buffer[0]);
            line templine;
            templine.x.append(x);
            templine.y.append(y);
            line_list.append(templine);
        }
        else
        {
            line_id=id_list.indexOf(rec_buffer[0]);
            collision->clearPosInfo(line_list[line_id].x.last(),line_list[line_id].y.last());
            line_list[line_id].x.append(x);
            line_list[line_id].y.append(y);
            ui->widget->graph(line_id)->setData(line_list[line_id].x,line_list[line_id].y,true);
        }
        collision->setCheckpoint(x,y,line_id);
        collision->start();
        ui->widget->replot();
    }
}

void MainWindow::emitStopSignal(quint8 line_id1,quint8 line_id2)
{
    quint8 id1=id_list[line_id1];
    quint8 id2=id_list[line_id2];
    quint8 sumh=(id1+id2)/255;
    quint8 suml=(id1+id2)%255;
    quint8 buffer[4]={id1,id2,sumh,suml};
    writeSerialport(buffer,sizeof(buffer)/sizeof(quint8));
}

quint8 MainWindow::addLine(quint8 id)
{
    quint8 temp_id=max_line_id;
    id_list.append(id);
    max_line_id++;
    ui->widget->addGraph();
    int b=temp_id%6;
    int g=temp_id/6;
    int r=temp_id/36;
    ui->widget->graph(temp_id)->setPen(QPen(QColor(r*51,g*51,b*51)));
    return temp_id;
}