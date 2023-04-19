#include "widget.h"
#include "ui_widget.h"
#include <QDebug>
#include <QThread>
#include <QMessageBox>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QFrame>
#include <QColorDialog>
#include <QtNetwork/QNetworkInterface>



#ifdef RK_3399_PLATFORM
#ifdef  __cplusplus
extern "C" {
#endif
#include "drv722.h"
//#include "cpu_mem_cal.h"
#ifdef  __cplusplus
}
#endif
#endif

#ifdef  __cplusplus
extern "C" {
#endif

#include "cpu_mem_cal.h"
#ifdef  __cplusplus
}
#endif

const char* g_build_time_str = "Buildtime :" __DATE__ " " __TIME__ ;   //获得编译时间

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define PAGES 8  //显示总页数



static Qt::Key keys_value[] = {Qt::Key_F1/*L1*/,Qt::Key_F2/*R1*/,Qt::Key_F3,Qt::Key_F4,Qt::Key_F5,Qt::Key_F6,Qt::Key_F7,Qt::Key_F8,
                              Qt::Key_F9,Qt::Key_F10,Qt::Key_Z/*L6*/,Qt::Key_X/*R6*/,Qt::Key_F11/*inc*/,Qt::Key_F12/*ext*/,
                               Qt::Key_0,Qt::Key_1,Qt::Key_2,Qt::Key_3,Qt::Key_4,Qt::Key_5,Qt::Key_6,
                              Qt::Key_7,Qt::Key_8,Qt::Key_9,Qt::Key_Asterisk/*\**/,Qt::Key_Slash/*#*/,
                                Qt::Key_C/*tel*/,Qt::Key_Control/*test*/,Qt::Key_Plus/*V+*/,Qt::Key_Minus/*V-*/,
                               Qt::Key_Up/*up*/,Qt::Key_Down/*down*/,Qt::Key_P/*Ptt*/
                              };
#ifdef RK_3399_PLATFORM
static int led_key_map[] = {
 1,2,3,4,5,6,7,8,9,10,44,45,11,12,27,18,19,20,21,22,23,24,25,26,28,29,13,14,35,36,30,31,17
};
#endif


struct system_config
{
    int is_cpu_stress_start;   //启动开始cpu压力测试？0表示不开启，1开启测试
    int is_gpio_flow_start;   //启动开启gpio流水灯吗？
    int is_key_lights_start;  //启动开启键灯吗？
    int is_cpu_test_checked;
    int is_mem_test_checked;
    int default_show_page;    //启动默认显示页面,默认是第一页
    int cpu_test_core_num;    //cpu的测试核心数
    int mem_test_usage;       //内存测试的百分比
}g_sys_conf;

void ReadConfigFile();
void SetConfigFile(void);


Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

    ReadConfigFile();   //读取配置文件

    //qDebug() << g_build_time_str;
    ui->label->setText(g_build_time_str);
    ui->label_2->setText("Create by dazhi-2023");

#ifdef RK_3399_PLATFORM
    //键灯控制
    timer_key_leds = new QTimer(this);
    connect ( timer_key_leds, SIGNAL ( timeout() ), this, SLOT ( timer_key_leds_slot_Function() ) );
    is_light_all_leds = 0;

#endif
    timer_net_stat = new QTimer(this);
    timer_cpu_mem_info  = new QTimer(this);
    connect ( timer_net_stat, SIGNAL ( timeout() ), this, SLOT ( timer_net_stat_slot_Function() ) );
    connect ( timer_cpu_mem_info, SIGNAL ( timeout() ), this, SLOT ( timer_cpu_mem_info_slot_Function() ) );

    ui->label_color->setVisible(false);
    //ui->label_color->installEventFilter(this);
    this->installEventFilter(this);
//    ui->lineEdit_interval->installEventFilter(this);


    page2_show_color = 0;
    is_test_press = 0;     //测试键没有按下
    key_light_connect = 1;  //键灯关联
    ui->stackedWidget->setCurrentIndex(0);

    intValidator = new QIntValidator;
    intValidator->setRange(1,999999);
    ui->lineEdit_interval->setValidator(intValidator);

    QRegExp rx("^([1-9]|[1-9]\\d|(1[0-9]\\d)|(2[0-4]\\d)|(2[0-5][0-4]))$");//输入范围为【1-254】
    pReg = new QRegExpValidator(rx, this);
    ui->lineEdit_ip1->setValidator(pReg);
    ui->lineEdit_ip2->setValidator(pReg);
    ui->lineEdit_ip3->setValidator(pReg);
    ui->textBrowser_ifconfig->setVisible(false);//显示ip信息的暂时不可见

#ifdef RK_3399_PLATFORM
    drvCoreBoardInit();
    drvSetLedBrt(100);   //led light max
    drvDimAllLED();

#endif
    lightpwm = 100;

    error_count[0] = 0;
    error_count[1] = 0;
    error_count[2] = 0;

    ping_status[0] = false;
    ping_status[1] = false;
    ping_status[2] = false;

    myprocess_ping[0] = nullptr;
    myprocess_ping[1] = nullptr;
    myprocess_ping[2] = nullptr;
    myprocess_play = nullptr;

    icmp_cur[0] = 0;
    icmp_cur[1] = 0;
    icmp_cur[2] = 0;
    icmp_saved[0] = 0;
    icmp_saved[1] = 0;
    icmp_saved[2] = 0;

    ui->label_ping_reson1->setText("");
    ui->label_ping_reson2->setText("");
    ui->label_ping_reson3->setText("");


    ui->radioButton_michand->setEnabled(false);
    ui->radioButton_micpanel->setEnabled(false);


    myprocess_FlowLEds = new QProcess;


    lcdPwm = 90;
    myprocess_iicspi = new QProcess;
    myprocess_ifconfig = new QProcess;
//    myprocess_top_cmd = new QProcess;
    myprocess_cpu_stress = new QProcess;
 //   connect(this->myprocess_top_cmd, SIGNAL(readyRead()),this,SLOT(topcmd_info_show()));//连接信号readyReadStandardOutput()
//    connect(this->myprocess_top_cmd, SIGNAL(finished(int)),this,SLOT(top_cmd_finished_slot(int)));//连接信号

    iicspi_connect = 0;

#ifdef RK_3399_PLATFORM
        myprocess_play = new QProcess;

        connect(this->myprocess_play, SIGNAL(finished(int)),this,SLOT(play_finished_slot(int)));//连接信号

#if 1
        myprocess_play1[0] = new QProcess;
        myprocess_play1[1] = new QProcess;
        connect(this->myprocess_play1[0], SIGNAL(finished(int)),this,SLOT(play_finished_slot(int)));//连接信号
        myprocess_play1[0]->setStandardOutputProcess(myprocess_play1[1]);
#endif
#endif
    on_radioButton_micpanel_clicked();


    ui->stackedWidget->setCurrentIndex(g_sys_conf.default_show_page);
    ui->comboBox_cpu->setCurrentIndex(g_sys_conf.cpu_test_core_num);
    ui->comboBox_memory->setCurrentIndex(g_sys_conf.mem_test_usage);
    ui->checkBox_mem_n->setChecked(g_sys_conf.is_mem_test_checked);
    ui->checkBox_cpu_n->setChecked(g_sys_conf.is_cpu_test_checked);
    ui->comboBox->setCurrentIndex(g_sys_conf.default_show_page);
    if(g_sys_conf.is_gpio_flow_start)
    {
        on_pushButton_FlowLEDS_clicked();
        ui->checkBox_gpio_flow->setChecked(true);
    }
    if(g_sys_conf.is_cpu_stress_start)
    {
        on_pushButton_start_cpustress_clicked();
        ui->checkBox_cpu_stress->setChecked(true);
    }
    if(g_sys_conf.is_key_lights_start)
    {
        on_pushButton_6_clicked();
        ui->checkBox_keyLights->setChecked(true);
    }
}






Widget::~Widget()
{
#ifdef RK_3399_PLATFORM
    drvCoreBoardExit();
    timer_key_leds->stop();
    delete timer_key_leds;

    if(myprocess_play->state() == QProcess::Running)
        myprocess_play->kill();
    delete myprocess_play;
#if 1
    if(myprocess_play1[0]->state() == QProcess::Running)
        myprocess_play1[0]->kill();
    delete myprocess_play1[0];
    if(myprocess_play1[1]->state() == QProcess::Running)
        myprocess_play1[1]->kill();
    delete myprocess_play1[1];
#endif
#endif

    if(myprocess_FlowLEds->state() == QProcess::Running)
        myprocess_FlowLEds->kill();
    delete myprocess_FlowLEds;
//    if(myprocess_top_cmd->state() == QProcess::Running)
//        myprocess_top_cmd->kill();
//    delete myprocess_top_cmd;

    if(myprocess_cpu_stress->state() == QProcess::Running)
        myprocess_cpu_stress->kill();
    delete myprocess_cpu_stress;

    delete  myprocess_iicspi;
    delete  myprocess_ifconfig;
    delete intValidator;
    delete pReg;
    delete timer_cpu_mem_info;
    delete timer_net_stat;
    delete ui;
}






void ReadConfigFile()
{
    QFile file("/home/deepin//qt_rk3399_config.txt");
    int index;

    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString line;
        QTextStream in(&file);  //用文件构造流

        qDebug() << line;
        while(!in.atEnd())//字符串有内容
        {
            line = in.readLine();//读取一行放到字符串里
            if(line.isNull())
                break;
            if(line.length() < 5)  //小于5个字节的，直接跳过
                continue;

            if(line.startsWith("#"))   //
                continue;

            else if(line.startsWith("cpu_stress_start"))   //查找关键字
            {
                g_sys_conf.is_cpu_stress_start = 1;
            }
            else if(line.startsWith("gpio_flow_start"))
            {
                g_sys_conf.is_gpio_flow_start = 1;
            }
            else if(line.startsWith("key_lights_start"))
            {
                g_sys_conf.is_key_lights_start = 1;
            }
            else if(line.startsWith("cpu_test_checked"))
            {
                g_sys_conf.is_cpu_test_checked = 1;
            }
            else if(line.startsWith("mem_test_checked"))
            {
                g_sys_conf.is_mem_test_checked = 1;
            }
            else if(line.startsWith("default_show_page"))
            {
                index = line.indexOf(":");
                if(index > 0)
                    g_sys_conf.default_show_page = line.mid(index+1).toInt();
                if(g_sys_conf.default_show_page > (PAGES-1) || g_sys_conf.default_show_page < 0)
                    g_sys_conf.default_show_page  = 0;
            }
            else if(line.startsWith("cpu_test_core_num"))
            {
                index = line.indexOf(":");
                if(index > 0)
                    g_sys_conf.cpu_test_core_num = line.mid(index+1).toInt();
                if(g_sys_conf.cpu_test_core_num > 5 || g_sys_conf.cpu_test_core_num < 0)
                    g_sys_conf.cpu_test_core_num  = 0;

            }
            else if(line.startsWith("mem_test_usage"))
            {
                index = line.indexOf(":");
                if(index > 0)
                    g_sys_conf.mem_test_usage = line.mid(index+1).toInt();
                if(g_sys_conf.mem_test_usage > 9 || g_sys_conf.mem_test_usage < 0)
                    g_sys_conf.mem_test_usage  = 0;
            }
        }
        file.close();
    }
    else
    {
        g_sys_conf.is_cpu_stress_start = 0;
        g_sys_conf.is_gpio_flow_start = 1;
        g_sys_conf.is_key_lights_start = 0;
        g_sys_conf.is_cpu_test_checked = 1;
        g_sys_conf.is_mem_test_checked = 1;
        g_sys_conf.default_show_page  = 0;
        g_sys_conf.cpu_test_core_num  = 0;
        g_sys_conf.mem_test_usage  = 0;
    }
}

//int is_cpu_test_checked;
//int is_mem_test_checked;

void SetConfigFile(void)
{
    QString strAll;
//    QString line;
    QStringList list;
    QFile qfile("/home/deepin//qt_rk3399_config.txt");

    //2.把内容填入
    if(qfile.open(QIODevice::WriteOnly | QIODevice::Text ))  //写入文件时替换，不存在时追加到最后一行
    {
        QTextStream stream(&qfile);
       if(g_sys_conf.is_cpu_stress_start)
       {
           stream << "cpu_stress_start\n";  //
       }
       if(g_sys_conf.is_gpio_flow_start)
       {
           stream << "gpio_flow_start\n";  //
       }
       if(g_sys_conf.is_key_lights_start)
       {
           stream << "key_lights_start\n";  //
       }
       if(g_sys_conf.is_cpu_test_checked)
       {
           stream << "cpu_test_checked\n";  //
       }
       if(g_sys_conf.is_mem_test_checked)
       {
           stream << "cpu_test_checked\n";  //
       }


       if(g_sys_conf.default_show_page > (PAGES-1) || g_sys_conf.default_show_page < 0)
           g_sys_conf.default_show_page  = 0;
       stream << "default_show_page:" << QString::number(g_sys_conf.default_show_page) <<"\n" ;  //

       if(g_sys_conf.cpu_test_core_num > 5 || g_sys_conf.cpu_test_core_num < 0)
           g_sys_conf.cpu_test_core_num  = 0;
       stream << "cpu_test_core_num:" << QString::number(g_sys_conf.cpu_test_core_num) <<"\n" ;  //


       if(g_sys_conf.mem_test_usage > 9 || g_sys_conf.mem_test_usage < 0)
           g_sys_conf.mem_test_usage  = 0;
       stream << "mem_test_usage:" << QString::number(g_sys_conf.mem_test_usage) <<"\n" ;  //

        qfile.close();
    }
    return ;
}








void Widget::Palette_button(int ison,int keyval)
{
    //注意与keys_value数组对应
    QToolButton* key_buttons[] = {ui->toolButton_L1,ui->toolButton_R1,ui->toolButton_L2,ui->toolButton_R2,ui->toolButton_L3,ui->toolButton_R3,ui->toolButton_L4,
                                   ui->toolButton_R4,ui->toolButton_L5,ui->toolButton_R5,ui->toolButton_L6,ui->toolButton_R6,ui->toolButton_Inc,ui->toolButton_Ext,
                                   ui->toolButton_0,ui->toolButton_1,ui->toolButton_2,ui->toolButton_3,ui->toolButton_4,ui->toolButton_5,ui->toolButton_6,
                                   ui->toolButton_7,ui->toolButton_8,ui->toolButton_9,ui->toolButton_11,ui->toolButton_12,ui->toolButton_Tell,ui->toolButton_Test,
                                   ui->toolButton_V1,ui->toolButton_V2,ui->toolButton_Up,ui->toolButton_Down,ui->toolButton_Ptt
                                   };
    QFrame* buttonFrame = new QFrame;
    QPalette p = buttonFrame->palette();
    if(ison)
        p.setColor(QPalette::Button,Qt::green);
    else
    {
        if(keyval == 0)
        {
            p.setColor(QPalette::Button,QColor(1, 1, 1, 1));//Qt::white);
            for(unsigned int i=0;i<ARRAY_SIZE(keys_value);i++)
            {
                key_buttons[i]->setPalette(p);
            }
            return;
        }
        else
            p.setColor(QPalette::Button,Qt::white);
    }
    if(is_test_press == 0)  //判断是否有组合键功能
    {
        for(unsigned int i=0;i<ARRAY_SIZE(keys_value);i++)
        {
            if(keyval == keys_value[i])
            {
                key_buttons[i]->setPalette(p);

#ifdef RK_3399_PLATFORM
//                qDebug() << " i = " <<i;
//                qDebug() << " key_map i = " << led_key_map[i];
                if(ison && key_light_connect)
                    drvLightLED(led_key_map[i]);
//                else
//                    drvDimLED(led_key_map[i]);
#endif
                break;
            }
        }
    }
    else
    {
#ifdef RK_3399_PLATFORM
            drvDimLED(14);   //test
#endif
    }

}


#ifdef RK_3399_PLATFORM
void Widget::light_off_all_leds(void)
{
    static bool stat = false;

    if(stat){
       drvDimAllLED();
       stat = false;
    }
    else
    {
        drvLightAllLED();
        stat = true;
    }
}


void Widget::circle_light_off_leds(void)
{
    static int i = 0;

    if(i<33)
        drvLightLED(led_key_map[i]);
    else if(i<66)
        drvDimLED(led_key_map[i-33]);
    else
    {
        i = 0;
        return;
    }
    i++;
}




//超时后的动作
void Widget::timer_key_leds_slot_Function()
{
    if(is_light_all_leds == 1)
    {
        light_off_all_leds();
    }
    else if(is_light_all_leds == 2)
    {
        circle_light_off_leds();
    }

}
#endif

//网络测试页，定时检测网络状态,500ms
void Widget::timer_net_stat_slot_Function()
{
    int i;
    QLabel* Ping_stat[3] = {ui->label_ping_stat1,ui->label_ping_stat2,ui->label_ping_stat3};
    getNetDeviceStats();

    for(i=0;i<3;i++)
    {
        if(ping_status[i])
        {
            if(icmp_saved[i] != icmp_cur[i])
            {
                icmp_saved[i] = icmp_cur[i];
            }
            else
            {
                if(Ping_stat[i]->text() != "异常")
                {
                    Ping_stat[i]->setText("异常");
                    Ping_stat[i]->setStyleSheet("QLabel{background-color:#ff0000;border-radius:5px;font: 20pt \"Ubuntu\";}");
                }
            }
        }
    }
}


void Widget::timer_cpu_mem_info_slot_Function()
{
    struct globals mem_info;
    int temp;
    int mem_usage = 0;
    int cpu_usage[7];
    jiffy_counts_t jifs[7];
    static long pre_busy[7] = {0};
    static long pre_total[7] = {0};
    int i;
    int cpu_freq[6];

    if(parse_meminfo(&mem_info) > 0)
    {
//        qDebug() << "mem_info.total_kb = " << mem_info.total_kb;
//        qDebug() << "mem_info.available_kb = " << mem_info.available_kb;
        mem_usage = 100 - mem_info.available_kb*100 / mem_info.total_kb;
        ui->label_mem_total->setText(QString::number(mem_info.total_kb/1000)+"MB");
        ui->label_mem_usage->setText(QString::number(mem_usage)+"%");
    }

    if(parse_cputemp(&temp) > 0)
    {
//        qDebug() << "cputemp = " << temp;
        ui->label_cpu_temp->setText(QString::number(temp));

    }
    if(parse_gputemp(&temp) > 0)
    {
//        qDebug() << "gputemp = " << temp;
        ui->label_gpu_temp->setText(QString::number(temp));
    }

    if(read_cpu_jiffy(jifs) > 6 )
    {
        for(i=0;i<7;i++)
        {
            cpu_usage[i] = (jifs[i].busy-pre_busy[i])*100/(jifs[i].total-pre_total[i]);
//            qDebug() << "cpu =" << i << " = " << cpu_usage[i];
            pre_busy[i] = jifs[i].busy;
            pre_total[i] = jifs[i].total;
        }
        ui->label_cpu_usage->setText(QString::number(cpu_usage[0]));
        ui->label_cpu0_usage->setText(QString::number(cpu_usage[1]));
        ui->label_cpu1_usage->setText(QString::number(cpu_usage[2]));
        ui->label_cpu2_usage->setText(QString::number(cpu_usage[3]));
        ui->label_cpu3_usage->setText(QString::number(cpu_usage[4]));
        ui->label_cpu4_usage->setText(QString::number(cpu_usage[5]));
        ui->label_cpu5_usage->setText(QString::number(cpu_usage[6]));
    }

    for(i=0;i<6;i++)
    {
        get_cpu_freq(i,&cpu_freq[i]);
    }
    ui->label_cpu0_freq->setText(QString::number(cpu_freq[0]));
    ui->label_cpu1_freq->setText(QString::number(cpu_freq[1]));
    ui->label_cpu2_freq->setText(QString::number(cpu_freq[2]));
    ui->label_cpu3_freq->setText(QString::number(cpu_freq[3]));
    ui->label_cpu4_freq->setText(QString::number(cpu_freq[4]));
    ui->label_cpu5_freq->setText(QString::number(cpu_freq[5]));

}




//label_color
bool Widget::eventFilter(QObject *obj, QEvent *event)
{
    obj = obj;
//    qDebug() << "eventFilter: " <<event->type();
//    qDebug() << "index" << ui->stackedWidget->currentIndex();

    if(event->type() == QEvent::MouseButtonRelease)
    {
        if((ui->stackedWidget->currentIndex() == 2))
        {
            QMouseEvent *e  = (QMouseEvent*)(event);
            QPoint sPoint2= e->pos();
     //       qDebug() << "sPoint2: " <<sPoint2.rx();
            if(sPoint2.rx() < 350)
            {
                last_color_page_show();
            }
            else if(sPoint2.rx() > 370)
            {
                next_color_page_show();
            }
        }
        return true;
    }
    else if(event->type() == QEvent::KeyPress)
    {
        QKeyEvent *KeyEvent = static_cast<QKeyEvent*>(event);


        if(ui->stackedWidget->currentIndex() == 0) //第一页，按键测试页
        {
            Palette_button(1,KeyEvent->key());
        }
        else if(ui->stackedWidget->currentIndex() == 2)//lcd颜色测试页，按键变换颜色
        {
            if(KeyEvent->key() == Qt::Key_Up)
            {
                if(ui->label_color->isVisible())
                {
                    last_color_page_show();
                }
                else
                {
                    lcdPwm = ui->horizontalScrollBar_light->value();
                    if(lcdPwm < 100)
                    {
                        lcdPwm += 5;
                        if(lcdPwm > 100)
                            lcdPwm = 100;
                        ui->horizontalScrollBar_light->setValue(lcdPwm);
                     //   ui->label_light_val->setText(QString::number(lcdPwm));
                    //    qDebug() << "lcdPwm = " << lcdPwm;
                    }
                }
            }
            else if(KeyEvent->key() == Qt::Key_Down)
            {
                if(ui->label_color->isVisible())
                {
                    next_color_page_show();
                }
                else
                {
                    lcdPwm = ui->horizontalScrollBar_light->value();
                    if(lcdPwm > 0)
                    {
                        lcdPwm -= 5;
                        if(lcdPwm < 0)
                            lcdPwm = 0;
                        ui->horizontalScrollBar_light->setValue(lcdPwm);
                     //   ui->label_light_val->setText(QString::number(lcdPwm));
                     //   qDebug() << "lcdPwm = " << lcdPwm;
                    }
                }
            }
            else if(KeyEvent->key() == Qt::Key_C)
            {
                on_pushButton_start_color_test_clicked();
            }
        }
        else if(ui->stackedWidget->currentIndex() == 1)  //键灯测试页
        {
            if(KeyEvent->key() == Qt::Key_F1)  // L1    Qt::Key_F1
            {
                ui->lineEdit_interval->backspace();
            }
            else if(KeyEvent->key() == Qt::Key_F3)  // L2    Qt::Key_F3
            {
                on_pushButton_6_clicked();
            }
            else if(KeyEvent->key() == Qt::Key_F5)  // L3    Qt::Key_F5
            {
                on_pushButton_7_clicked();
            }
            else if(KeyEvent->key() == Qt::Key_F7)  // L4    Qt::Key_F7
            {
                on_pushButton_clicked();
            }
            else if(KeyEvent->key() == Qt::Key_Up)
            {
                if(lightpwm < 100){
                    lightpwm +=5;
                    if(lightpwm > 100)
                        lightpwm = 100;
                 }
                ui->verticalScrollBar_lightpwm2->setValue(lightpwm);

            }
            else if(KeyEvent->key() == Qt::Key_Down)
            {
                if(lightpwm > 0){
                    lightpwm -=5;
                    if(lightpwm < 0 )
                        lightpwm = 0;
                 }
                ui->verticalScrollBar_lightpwm2->setValue(lightpwm);//drvSetLedBrt(lightpwm);
            }
        }

        else if(ui->stackedWidget->currentIndex() == 3)  //网络测试页
        {
            if(KeyEvent->key() == Qt::Key_F1)  // L1    Qt::Key_F1
            {
                if(ui->lineEdit_ip1 == qobject_cast<QLineEdit*>(ui->stackedWidget->focusWidget()))
                {
                    ui->lineEdit_ip1->backspace();
                }
                else if(ui->lineEdit_ip2 == qobject_cast<QLineEdit*>(ui->stackedWidget->focusWidget()))
                {
                    ui->lineEdit_ip2->backspace();
                }
                else if(ui->lineEdit_ip3 == qobject_cast<QLineEdit*>(ui->stackedWidget->focusWidget()))
                {
                    ui->lineEdit_ip3->backspace();
                }

            }
            else if(KeyEvent->key() == Qt::Key_F5)
            {
                if(ui->pushButton_2->isEnabled())
                    on_pushButton_2_clicked();
            }
            else if(KeyEvent->key() == Qt::Key_F7)
            {
                if(ui->pushButton_4->isEnabled())
                    on_pushButton_4_clicked();
            }
            else if(KeyEvent->key() == Qt::Key_F9)
            {
                if(ui->pushButton_5->isEnabled())
                    on_pushButton_5_clicked();
            }
            else if(KeyEvent->key() == Qt::Key_F11)   //内通按键 配置ip
            {
                on_pushButton_3_clicked();
            }
            else if(KeyEvent->key() == Qt::Key_F12)    //外通按键，查看ip
            {
                on_pushButton_ifconfig_clicked();
            }
            else if(KeyEvent->key() == Qt::Key_F2)
            {
                if(ui->lineEdit_ip1 == qobject_cast<QLineEdit*>(ui->stackedWidget->focusWidget()))
                {
                    ui->lineEdit_ip2->setFocus();
                }
                else if(ui->lineEdit_ip2 == qobject_cast<QLineEdit*>(ui->stackedWidget->focusWidget()))
                {
                    ui->lineEdit_ip3->setFocus();
                }
                else if(ui->lineEdit_ip3 == qobject_cast<QLineEdit*>(ui->stackedWidget->focusWidget()))
                {
                    ui->lineEdit_ip3->clearFocus();
                }
                else
                {
                    ui->lineEdit_ip1->setFocus();
                }

            }
            else if(KeyEvent->key() == Qt::Key_F6)    //调整大包，自适应
            {
                if(!ping_status[0])
                {
                    if(!ui->checkBox_bigpack1->isChecked() && !ui->checkBox_adap1->isChecked())
                        ui->checkBox_bigpack1->setChecked(true);
                    else if(ui->checkBox_bigpack1->isChecked() && !ui->checkBox_adap1->isChecked())
                        ui->checkBox_adap1->setChecked(true);
                    else if(ui->checkBox_bigpack1->isChecked() && ui->checkBox_adap1->isChecked())
                        ui->checkBox_bigpack1->setChecked(false);
                    else if(!ui->checkBox_bigpack1->isChecked() && ui->checkBox_adap1->isChecked())
                        ui->checkBox_adap1->setChecked(false);
                }
            }
            else if(KeyEvent->key() == Qt::Key_F8)    //调整大包，自适应
            {
                if(!ping_status[1])
                {
                    if(!ui->checkBox_bigpack2->isChecked() && !ui->checkBox_adap2->isChecked())
                        ui->checkBox_bigpack2->setChecked(true);
                    else if(ui->checkBox_bigpack2->isChecked() && !ui->checkBox_adap2->isChecked())
                        ui->checkBox_adap2->setChecked(true);
                    else if(ui->checkBox_bigpack2->isChecked() && ui->checkBox_adap2->isChecked())
                        ui->checkBox_bigpack2->setChecked(false);
                    else if(!ui->checkBox_bigpack2->isChecked() && ui->checkBox_adap2->isChecked())
                        ui->checkBox_adap2->setChecked(false);
                }
            }
            else if(KeyEvent->key() == Qt::Key_F10)    //调整大包，自适应
            {
                if(!ping_status[2])
                {
                    if(!ui->checkBox_bigpack3->isChecked() && !ui->checkBox_adap3->isChecked())
                        ui->checkBox_bigpack3->setChecked(true);
                    else if(ui->checkBox_bigpack3->isChecked() && !ui->checkBox_adap3->isChecked())
                        ui->checkBox_adap3->setChecked(true);
                    else if(ui->checkBox_bigpack3->isChecked() && ui->checkBox_adap3->isChecked())
                        ui->checkBox_bigpack3->setChecked(false);
                    else if(!ui->checkBox_bigpack3->isChecked() && ui->checkBox_adap3->isChecked())
                        ui->checkBox_adap3->setChecked(false);
                }
            }
        }
        else if(ui->stackedWidget->currentIndex() == 4)//音频测试页
        {
            int val;
            if(KeyEvent->key() == Qt::Key_F1)
            {
                if(is_test_press == 1)
                {
                    if(ui->radioButton_SpeakVol->isChecked())
                    {
                        ui->radioButton_SpeakVol->setChecked(false);
#ifdef RK_3399_PLATFORM
                        //drvSetSpeakVolume(value);
#endif
                    }
                    else
                    {
                        ui->radioButton_SpeakVol->setChecked(true);
#ifdef RK_3399_PLATFORM
                        //drvSetSpeakVolume(value);
#endif
                    }
                }
                else
                {
                    val = ui->horizontalScrollBar_SpeakVol->value();
                    if(val > 0)
                    {
                        val -= 5;
                        if(val < 0)
                            val = 0;
                    }
                    ui->horizontalScrollBar_SpeakVol->setValue(val);
                }

            }
            else if(KeyEvent->key() == Qt::Key_F3)
            {
                if(is_test_press == 1)
                {
                    if(ui->radioButton_HandVol->isChecked())
                    {
                        ui->radioButton_HandVol->setChecked(false);
#ifdef RK_3399_PLATFORM
                        drvEnableHandout();
#endif
                    }
                    else
                    {
                        ui->radioButton_HandVol->setChecked(true);
#ifdef RK_3399_PLATFORM
                        drvDisableHandout();
#endif
                    }
                }
                else
                {
                    val = ui->horizontalScrollBar_HandVol->value();
                    if(val > 0)
                    {
                        val -= 5;
                        if(val < 0)
                            val = 0;
                    }
                    ui->horizontalScrollBar_HandVol->setValue(val);
                }
            }
            else if(KeyEvent->key() == Qt::Key_F5)
            {
                if(is_test_press == 1)
                {
                    if(ui->radioButton_EarphVol->isChecked())
                    {
                        ui->radioButton_EarphVol->setChecked(false);
#ifdef RK_3399_PLATFORM
                        drvEnableEarphout();
#endif
                    }
                    else
                    {
                        ui->radioButton_EarphVol->setChecked(true);
#ifdef RK_3399_PLATFORM
                        drvDisableEarphout();
#endif
                    }
                }
                else
                {
                    val = ui->horizontalScrollBar_EarphVol->value();
                    if(val > 0)
                    {
                        val -= 5;
                        if(val < 0)
                            val = 0;
                    }
                    ui->horizontalScrollBar_EarphVol->setValue(val);
                }
            }
            else if(KeyEvent->key() == Qt::Key_F2)
            {
                val = ui->horizontalScrollBar_SpeakVol->value();
                if(val < 100)
                {
                    val += 5;
                    if(val > 100)
                        val = 100;
                }
                ui->horizontalScrollBar_SpeakVol->setValue(val);

            }
            else if(KeyEvent->key() == Qt::Key_F4)
            {
                val = ui->horizontalScrollBar_HandVol->value();
                if(val < 100)
                {
                    val += 5;
                    if(val > 100)
                        val = 100;
                }
                ui->horizontalScrollBar_HandVol->setValue(val);
            }
            else if(KeyEvent->key() == Qt::Key_F6)
            {
                val = ui->horizontalScrollBar_EarphVol->value();
                if(val < 100)
                {
                    val += 5;
                    if(val > 100)
                        val = 100;
                }
                ui->horizontalScrollBar_EarphVol->setValue(val);
            }
            else if(KeyEvent->key() == Qt::Key_F9)
            {
                if(ui->radioButton_rec->isChecked())
                {
                    ui->radioButton_loop->setChecked(true);
                    ui->radioButton_rec->setChecked(false);

                }
                else if(ui->radioButton_loop->isChecked())
                {
                    ui->radioButton_loop->setChecked(false);
                    ui->radioButton_playrec->setChecked(true);
                    ui->radioButton_michand->setEnabled(false);
                    ui->radioButton_micpanel->setEnabled(false);
                }
                else if(ui->radioButton_playrec->isChecked())
                {
                    ui->radioButton_playrec->setChecked(false);
                    ui->radioButton_playmusic->setChecked(true);

                }
                else if(ui->radioButton_playmusic->isChecked())
                {
                    ui->radioButton_playmusic->setChecked(false);
                    ui->radioButton_rec->setChecked(true);
                    ui->radioButton_michand->setEnabled(true);
                    ui->radioButton_micpanel->setEnabled(true);
                }
            }
            else if(KeyEvent->key() == Qt::Key_F10)
            {
                if(ui->radioButton_rec->isChecked() || ui->radioButton_loop->isChecked())
                {
                    if(ui->radioButton_michand->isChecked())
                    {
                        ui->radioButton_michand->setChecked(false);
                        ui->radioButton_micpanel->setChecked(true);
#ifdef RK_3399_PLATFORM
                        drvSelectHandFreeMic();
#endif
                    }
                    else if(ui->radioButton_micpanel->isChecked())
                    {
                        ui->radioButton_micpanel->setChecked(false);
                        ui->radioButton_michand->setChecked(true);
#ifdef RK_3399_PLATFORM
                        drvSelectHandMic();
#endif
                    }
                }
            }
            else if(KeyEvent->key() == Qt::Key_C)
            {
                on_pushButton_Play_clicked();
            }

        }
        else if(ui->stackedWidget->currentIndex() == 5)//触摸屏测试页
        {
            if(KeyEvent->key() == Qt::Key_C) //拨号/电话键
            {
                on_pushButton_start_lcd_touch_clicked();
            }
        }



        if(is_test_press == 1)  //判断是否有组合键功能
        {
            if(KeyEvent->key() == Qt::Key_4)
            {
                next_func_page_show();
            }
            else if(KeyEvent->key() == Qt::Key_1)
            {
                this->close();
            }
            else if(KeyEvent->key() == Qt::Key_7)
            {
                last_func_page_show();
            }
            else if(KeyEvent->key() == Qt::Key_2)
            {
                if(ui->stackedWidget->currentIndex() == 0)
                {
                    if(ui->checkBox->isChecked())
                    {
                        ui->checkBox->setChecked(false);
                    }
                    else
                        ui->checkBox->setChecked(true);
                }
            }

        }


        if(KeyEvent->key() == Qt::Key_Control)
            is_test_press = 1;     //测试键按下

        return true;
    }
    else if(event->type() == QEvent::KeyRelease)
    {
        QKeyEvent *KeyEvent = static_cast<QKeyEvent*>(event);
        if(KeyEvent->key() == Qt::Key_Control)
            is_test_press = 0;     //测试键没有按下

        if((ui->stackedWidget->currentIndex() == 0))
        {
            Palette_button(0,KeyEvent->key());
        }
        return true;\
    }
    return false;
}


//下一项
void Widget::next_func_page_show()
{
    int index = ui->stackedWidget->currentIndex();

#ifdef RK_3399_PLATFORM
    if(index == 0)
        drvDimAllLED();
#endif

    if(index < (PAGES-1) )
        index ++;

    if(index == 3){  //进入网络测试页，开启定时器
        timer_net_stat->start(500);
        ui->pushButton_5->setEnabled(false);
        ui->pushButton_2->setEnabled(false);
        ui->pushButton_4->setEnabled(false);
    }
    else       //退出网络测试页关闭定时器
        timer_net_stat->stop();

    if(index == 7){  //进入网络测试页，开启定时器
        timer_cpu_mem_info->start(1000);
    }
    else       //退出网络测试页关闭定时器
        timer_cpu_mem_info->stop();
    ui->stackedWidget->setCurrentIndex(index);


}
//上一项
void Widget::last_func_page_show()
{
    int index = ui->stackedWidget->currentIndex();

    if(index > 0 )
        index --;
#ifdef RK_3399_PLATFORM
    if(index == 0){
        drvDimAllLED();
        Palette_button(0,0);//键恢复为灰色
    }
#endif

    if(index == 3){  //进入网络测试页，开启定时器
        timer_net_stat->start(500);
        ui->pushButton_5->setEnabled(false);
        ui->pushButton_2->setEnabled(false);
        ui->pushButton_4->setEnabled(false);
    }
    else       //退出网络测试页关闭定时器
        timer_net_stat->stop();

    if(index == 7){  //进入网络测试页，开启定时器
        timer_cpu_mem_info->start(1000);
    }
    else       //退出网络测试页关闭定时器
        timer_cpu_mem_info->stop();

    ui->stackedWidget->setCurrentIndex(index);
}



void Widget::closeEvent(QCloseEvent *event)
{
//    QMessageBox box;
//    //设置文本框的大小
//    box.setStyleSheet("QLabel{"
//                          "min-width:500px;"
//                          "min-height:300px; "
//                          "font-size:40px;"
//                          "}");
// //   box.set
//    box.setText("确定要退出吗？");
//    box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
//    if(box.exec() == QMessageBox::Ok)
    //if(QMessageBox::information(this, "提示","确定要退出吗？", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
    {
        event->accept();
        qApp->quit();
    }
//    else
//    {
//        event->ignore();
//    }

}




//page1  next
void Widget::on_pushButton_Next1_clicked()
{
    next_func_page_show();  //page2
}

//page2 last
void Widget::on_pushButton_Last1_clicked()
{
    last_func_page_show(); //ui->stackedWidget->setCurrentIndex(0);  //page1
}


//page2 start test lcd color
void Widget::on_pushButton_start_color_test_clicked()
{
//    ui->pushButton_start_color_test->setVisible(false);
//    ui->pushButton_Exit2->setVisible(false);
//    ui->pushButton_Last1->setVisible(false);
//    ui->pushButton_Next2->setVisible(false);
//    ui->label_Page2_title->setVisible(false);
    ui->label_color->setVisible(true);
//    ui->label_4->setVisible(false);
//    ui->label_10->setVisible(false);
//    ui->label_11->setVisible(false);
//    ui->label_12->setVisible(false);
//    ui->textBrowser->setVisible(false);
//    ui->label_23->setVisible(false);
//     timer_color->start(2000);  //2s
    ui->label_color->setStyleSheet("background-color:rgb(255,0,0)");
    ui->label_color->raise();
#ifdef RK_3399_PLATFORM
    drvSetLcdBrt(100);
#endif

}


//page2 next page
void Widget::on_pushButton_Next2_clicked()
{
    next_func_page_show();  //page2ui->stackedWidget->setCurrentIndex(2);  //page2
}


//close
void Widget::on_pushButton_Exit1_clicked()
{
    this->close();
}


//page3 left
void Widget::last_color_page_show()
{
//    qDebug() << "on_pushButton_P3_l_clicked";
    if(page2_show_color <= 0)
    {
        page2_show_color = 0;
        return;
    }
    else if(page2_show_color == 1)
    {
        page2_show_color = 0;
        ui->label_color->setStyleSheet("background-color:rgb(255,0,0)");
    }
    else if(page2_show_color == 2)
    {
        page2_show_color = 1;
        ui->label_color->setStyleSheet("background-color:rgb(0,255,0)");

    }
    else if(page2_show_color == 3)
    {
        page2_show_color = 2;
        ui->label_color->setStyleSheet("background-color:rgb(0,0,255)");
    }
}


//page3 right
void Widget::next_color_page_show()
{
//    qDebug() << "on_pushButton_P3_r_clicked";

    if(page2_show_color >= 3)
    {
     //   page3_show_color = 3;
//        ui->pushButton_start_color_test->setVisible(true);
//        ui->pushButton_Exit2->setVisible(true);
//        ui->pushButton_Last1->setVisible(true);
//        ui->pushButton_Next2->setVisible(true);
//        ui->label_Page2_title->setVisible(true);
        ui->label_color->setVisible(false);
        ui->label_color->lower();
//        ui->label_4->setVisible(true);
//        ui->label_10->setVisible(true);
//        ui->label_11->setVisible(true);
//        ui->label_12->setVisible(true);
//        ui->textBrowser->setVisible(true);
//        ui->label_23->setVisible(true);
        page2_show_color = 0;
        return;
    }
    else if(page2_show_color == 0)
    {
        page2_show_color = 1;
        ui->label_color->setStyleSheet("background-color:rgb(0,255,0)");
    }
    else if(page2_show_color == 1)
    {
        page2_show_color = 2;
        ui->label_color->setStyleSheet("background-color:rgb(0,0,255)");

    }
    else if(page2_show_color == 2)
    {
        page2_show_color = 3;
        ui->label_color->setStyleSheet("background-color:rgb(255,255,255)");
    }
}


//page 0 : 键灯全部点亮
void Widget::on_pushButton_clicked()
{
    if(ui->pushButton->text() == "3.键灯全部点亮"){
        ui->pushButton->setText("3.键灯全部熄灭");
        ui->lineEdit_interval->clearFocus();
        ui->pushButton->setStyleSheet("QPushButton{background-color:#ff0000;font: 20pt \"Ubuntu\";}");
#ifdef RK_3399_PLATFORM
       drvLightAllLED();
#endif
    }
    else{
        ui->pushButton->setStyleSheet("QPushButton{background-color:#00ff00;font: 20pt \"Ubuntu\";}");
        ui->lineEdit_interval->clearFocus();
        ui->pushButton->setText("3.键灯全部点亮");
#ifdef RK_3399_PLATFORM
       drvDimAllLED();
#endif
    }
}



void Widget::on_pushButton_FlowLEDS_clicked()
{
    if(ui->pushButton_FlowLEDS->text() == "4.工装板流水灯"){
        ui->pushButton_FlowLEDS->setText("4.工装板流水灯结束");
        ui->lineEdit_interval->clearFocus();
        ui->pushButton_FlowLEDS->setStyleSheet("QPushButton{background-color:#ff0000;font: 20pt \"Ubuntu\";}");
#ifdef RK_3399_PLATFORM
        myprocess_FlowLEds->start("/home/deepin/leds_test",QIODevice::ReadOnly);
    //   drvLightAllLED();
#endif
    }
    else{
        ui->lineEdit_interval->clearFocus();
        ui->pushButton_FlowLEDS->setText("4.工装板流水灯");
        ui->pushButton_FlowLEDS->setStyleSheet("QPushButton{background-color:#00ff00;font: 20pt \"Ubuntu\";}");
#ifdef RK_3399_PLATFORM
        myprocess_FlowLEds->kill();
     //  drvDimAllLED();
#endif
    }
}








//开始触摸测试
void Widget::on_pushButton_start_lcd_touch_clicked()
{
    lcd_touch_ui = new fingerpaint(this);
    lcd_touch_ui->show();
}


void Widget::play_finished_slot(int ret)
{
    Q_UNUSED(ret);

    qDebug()  << "play_finished_slot";

//    if(myprocess_play)
//    {
//        delete myprocess_play;
//        myprocess_play = nullptr;
//    }

    ui->radioButton_rec->setEnabled(true);
    ui->radioButton_loop->setEnabled(true);
    ui->radioButton_playrec->setEnabled(true);
    ui->radioButton_playmusic->setEnabled(true);
    if(ui->radioButton_rec->isChecked() || ui->radioButton_loop->isChecked() )
    {
        ui->radioButton_michand->setEnabled(true);
        ui->radioButton_micpanel->setEnabled(true);
    }
    ui->pushButton_Play->setText("开始(拨号键)");

}




//音频测试页：播放按键
void Widget::on_pushButton_Play_clicked()
{
    static int loop_flag = 0;
    QString cmd;
    qDebug()  << "on_pushButton_Play_clicked";

    if(ui->pushButton_Play->text() == "开始(拨号键)"){
        ui->pushButton_Play->setStyleSheet("QPushButton{background-color:#ff0000;font: 20pt \"Ubuntu\";}");
        if(ui->radioButton_rec->isChecked())
        {
            cmd = "arecord -f cd  /home/deepin/test.wav";
            qDebug()  << "录音测试" ;
            loop_flag = 0;
        }
        else if(ui->radioButton_loop->isChecked())
        {
#ifdef RK_3399_PLATFORM
#if 1
            myprocess_play1[0]->start("arecord  -f cd");
            myprocess_play1[1]->start("aplay");
#else
            myprocess_play->start("i2cset -f -y 4 0x10 39 0x40");
            myprocess_play->waitForFinished();
#endif
#endif
            loop_flag = 1;
            qDebug()  << "回环测试" ;
        }
        else if(ui->radioButton_playrec->isChecked())
        {
            cmd = "aplay /home/deepin/test.wav";
            qDebug()  << "播放录音" ;
            loop_flag = 0;
        }
        else if(ui->radioButton_playmusic->isChecked())
        {
            cmd = "aplay /home/deepin/123.wav";
            qDebug()  << "播放音乐" ;
            loop_flag = 0;
        }

#ifdef RK_3399_PLATFORM
        qDebug()<<"cmd = " << cmd;
        if(!loop_flag)
        {           
            myprocess_play->start(cmd,QIODevice::ReadOnly);//ReadOnly,ReadWrite
        }

#endif
        ui->radioButton_rec->setEnabled(false);
        ui->radioButton_loop->setEnabled(false);
        ui->radioButton_playrec->setEnabled(false);
        ui->radioButton_playmusic->setEnabled(false);
        ui->radioButton_michand->setEnabled(false);
        ui->radioButton_micpanel->setEnabled(false);
        ui->pushButton_Play->setText("结束");
    }
    else{
        qDebug()  << "on_pushButton_Play_clicked  else";
        ui->pushButton_Play->setStyleSheet("QPushButton{background-color:#00ff00;font: 20pt \"Ubuntu\";}");
#ifdef RK_3399_PLATFORM
        if(myprocess_play->state()==QProcess::Running)
            myprocess_play->kill();
#else
        play_finished_slot(0);
#endif
        if(loop_flag)//子进程杀不死，暂时这么处理吧
        {
#ifdef RK_3399_PLATFORM
#if 1
            if(myprocess_play1[0]->state()==QProcess::Running)
                myprocess_play1[0]->kill();
            if(myprocess_play1[1]->state()==QProcess::Running)
                myprocess_play1[1]->kill();
#else
            myprocess_play->start("i2cset -f -y 4 0x10 39 0x80");
            myprocess_play->waitForFinished();
#endif
#endif
            loop_flag = 0;
        }
//        ui->radioButton_rec->setEnabled(true);
//        ui->radioButton_loop->setEnabled(true);
//        ui->radioButton_playrec->setEnabled(true);
//        ui->radioButton_playmusic->setEnabled(true);
//        ui->pushButton_Play->setText("开始(电话键)");
    }
}


//音频测试页：键灯全部点亮熄灭控制
void Widget::on_pushButton_6_clicked()
{
#ifdef RK_3399_PLATFORM
    int time_out = 0;
#endif
    if(ui->pushButton_6->text() == "1.键灯全部点亮熄灭控制"){
        ui->pushButton_6->setText("结束");
        ui->pushButton_7->setEnabled(false);
        ui->pushButton->setEnabled(false);
        ui->lineEdit_interval->clearFocus();
        ui->pushButton_6->setStyleSheet("QPushButton{background-color:#ff0000;font: 20pt \"Ubuntu\";}");
#ifdef RK_3399_PLATFORM
        //键灯控制
        time_out = ui->lineEdit_interval->text().toInt();
        if(time_out < 10)
            time_out = 10;
        timer_key_leds->start(time_out);
        is_light_all_leds = 1;
#endif
     }
     else{
        ui->pushButton_6->setStyleSheet("QPushButton{background-color:#00ff00;font: 20pt \"Ubuntu\";}");
        ui->pushButton_6->setText("1.键灯全部点亮熄灭控制");
        ui->pushButton_7->setEnabled(true);
        ui->pushButton->setEnabled(true);
        ui->lineEdit_interval->clearFocus();
#ifdef RK_3399_PLATFORM
        //键灯控制
        timer_key_leds->stop();
        is_light_all_leds = 0;
#endif
    }
}

//音频测试页：键灯流水灯控制
void Widget::on_pushButton_7_clicked()
{
#ifdef RK_3399_PLATFORM
    int time_out = 0;
#endif
    if(ui->pushButton_7->text() == "2.键灯流水灯控制"){
        ui->pushButton_7->setStyleSheet("QPushButton{background-color:#ff0000;font: 20pt \"Ubuntu\";}");
        ui->pushButton_7->setText("结束");
        ui->pushButton_6->setEnabled(false);
        ui->pushButton->setEnabled(false);
        ui->lineEdit_interval->clearFocus();
#ifdef RK_3399_PLATFORM
        //键灯控制
        time_out = ui->lineEdit_interval->text().toInt();
        if(time_out < 10)
            time_out = 10;
        timer_key_leds->start(time_out);
        is_light_all_leds = 2;
#endif
    }
    else{
        ui->pushButton_7->setText("2.键灯流水灯控制");
        ui->pushButton_7->setStyleSheet("QPushButton{background-color:#00ff00;font: 20pt \"Ubuntu\";}");
        ui->pushButton_6->setEnabled(true);
        ui->pushButton->setEnabled(true);
        ui->lineEdit_interval->clearFocus();
#ifdef RK_3399_PLATFORM
        //键灯控制
        timer_key_leds->stop();
        is_light_all_leds = 0;

#endif
    }
}





//bool CheckNetInfo(QString netdev_name)
//{
//    QNetworkInterface net= QNetworkInterface::interfaceFromName(netdev_name);
//    if(net.flags().testFlag(QNetworkInterface::IsUp)
//            && net.flags().testFlag(QNetworkInterface::IsRunning)
//            && !net.flags().testFlag(QNetworkInterface::IsLoopBack)
//            && (net.name() == netdev_name))
//    {
//        qDebug() << "eth0 is Connected" << endl;
//        return true;
//    }
//    else
//    {
//        qDebug() << "eth0 is Not Connected";
//        return false;
//    }
//}



static bool isipAddr_sameSegment(const QString & ip1,const QString & ip2)
{
    int i,j;

    if (ip1.isEmpty() || ip1.isEmpty())
    {
        return false;
    }

    i = ip1.lastIndexOf('.');
    j = ip2.lastIndexOf('.');

    if(i == j)  //长度是否相同
    {
        QString str1 = ip1.mid(0,i);
        QString str2 = ip2.mid(0,i);

//        qDebug()<< str1;
//        qDebug()<< str2;
        if(str1 == str2)  //相等吗
        {
            return true;
        }

    }
    return false;
}



void Widget::getNetDeviceStats()
{
    QList<QNetworkInterface> list;
    QList<QNetworkAddressEntry> list_addrs;
    QNetworkInterface intf;
    list = QNetworkInterface::allInterfaces(); //获取系统里所有的网卡对象

   // QLabel *net_stat[] = {ui->label_Net_Stat1,ui->label_Net_Stat2,ui->label_Net_Stat3};

    for (int i = 0; i < list.size(); i++)
    {
        intf = list.at(i);
        if(intf.name() == "lo")
            continue;
        if(intf.flags() & intf.IsRunning){
            if(intf.name() == "enp1s0f0"){
                ui->label_Net_Stat1->setText("已连接");
                ui->label_Net_Stat1->setStyleSheet("QLabel{background-color:#00ff00;border-radius:5px;font: 20pt \"Ubuntu\";}");
                if(isipAddr_sameSegment(intf.addressEntries().at(0).ip().toString(),"192.168.0.200"))
                {
                    ui->pushButton_2->setEnabled(true);
                    ui->label_ping_reson1->setText("");
                }
                else
                {                    ui->pushButton_2->setEnabled(false);
                    ui->label_ping_reson1->setText("设备ip与配测计算机网段不同，请点击\"配置rk3399主板IP\"按钮");
                }
            }
            else if(intf.name() == "enp1s0f1"){
                ui->label_Net_Stat2->setText("已连接");
                ui->label_Net_Stat2->setStyleSheet("QLabel{background-color:#00ff00;border-radius:5px;font: 20pt \"Ubuntu\";}");
                if(isipAddr_sameSegment(intf.addressEntries().at(0).ip().toString(),"192.168.1.200"))
                {
                    ui->pushButton_4->setEnabled(true);
                    ui->label_ping_reson2->setText("");
                }
                else
                {
                    ui->pushButton_4->setEnabled(false);
                    ui->label_ping_reson2->setText("设备ip与配测计算机网段不同，请点击\"配置rk3399主板IP\"按钮");
                }
            }
            else if(intf.name() == "eth2"){
                ui->label_Net_Stat3->setText("已连接");
                ui->label_Net_Stat3->setStyleSheet("QLabel{background-color:#00ff00;border-radius:5px;font: 20pt \"Ubuntu\";}");
                if(isipAddr_sameSegment(intf.addressEntries().at(0).ip().toString(),"192.168.2.200"))
                {
                    ui->pushButton_5->setEnabled(true);
                    ui->label_ping_reson3->setText("");
                }
                else
                {
                    ui->pushButton_5->setEnabled(false);
                    ui->label_ping_reson3->setText("设备ip与配测计算机网段不同，请点击\"配置rk3399主板IP\"按钮");
                }
            }
        }
        else
        {
            if(intf.name() == "enp1s0f0"){
                if(ui->label_Net_Stat1->text() != "已断开")
                {
                    ui->label_Net_Stat1->setText("已断开");
                    ui->label_Net_Stat1->setStyleSheet("QLabel{background-color:#ff0000;border-radius:5px;font: 20pt \"Ubuntu\";}");
                    ui->pushButton_2->setEnabled(false);
                    ui->label_ping_reson1->setText("网线已断开，请连接网线");
                    if(ping_status[0])
                    {
                        terminate_ping1();
                    }
                }
            }
            else if(intf.name() == "enp1s0f1"){
                if(ui->label_Net_Stat2->text() != "已断开")
                {
                    ui->label_Net_Stat2->setText("已断开");
                    ui->label_Net_Stat2->setStyleSheet("QLabel{background-color:#ff0000;border-radius:5px;font: 20pt \"Ubuntu\";}");
                    ui->pushButton_4->setEnabled(false);
                    ui->label_ping_reson2->setText("网线已断开，请连接网线");
                    if(ping_status[1])
                    {
                        terminate_ping2();
                    }
                }
            }
            else if(intf.name() == "eth2"){
                if(ui->label_Net_Stat3->text() != "已断开")
                {
                    ui->label_Net_Stat3->setText("已断开");
                    ui->label_Net_Stat3->setStyleSheet("QLabel{background-color:#ff0000;border-radius:5px;font: 20pt \"Ubuntu\";}");
                    ui->pushButton_5->setEnabled(false);
                    ui->label_ping_reson3->setText("网线已断开，请连接网线");
                    if(ping_status[2])
                    {
                        terminate_ping3();
                    }
                }
            }
        }

//        qDebug() << intf.name() << "  " << intf.flags(); // 获取网卡的状态，　是否激活,能否广播组播等　.
//        qDebug() << "IsUp" << intf.IsUp;
//        qDebug() << "IsRunning" << intf.IsRunning;
//        list_addrs = intf.addressEntries(); // 一个网卡可以设多个地址，获取当前网卡对象的所有ip地址
//        for (int j = 0; j < list_addrs.size(); j++)
//            qDebug() << list_addrs.at(j).ip().toString();

//        qDebug() << "###############";
    }
}


void Widget::terminate_ping1()
{
    if(myprocess_ping[0] != nullptr)
        myprocess_ping[0]->kill();

    ui->pushButton_2->setText("ping enp1s0f0");
    ui->checkBox_bigpack1->setEnabled(true);
    ui->checkBox_adap1->setEnabled(true);
    ping_status[0] = false;
}
void Widget::terminate_ping2()
{
    if(myprocess_ping[1] != nullptr)
        myprocess_ping[1]->kill();

    ui->pushButton_4->setText("ping enp1s0f1");
    ui->checkBox_bigpack2->setEnabled(true);
    ui->checkBox_adap2->setEnabled(true);
    ping_status[1] = false;

}
void Widget::terminate_ping3()
{
    if(myprocess_ping[2] != nullptr)
        myprocess_ping[2]->kill();

    ui->pushButton_5->setText("ping eth2");
    ui->checkBox_bigpack3->setEnabled(true);
    ui->checkBox_adap3->setEnabled(true);
    ping_status[2] = false;

}

void Widget::ping_info_show(QString &strMsg,int ping_num)
{
    //QString strMsg = myprocess_ping1->readAllStandardOutput();
    QLabel* Ping_stat[3] = {ui->label_ping_stat1,ui->label_ping_stat2,ui->label_ping_stat3};
    QLabel* ping_err[3] = {ui->label_ping_err1,ui->label_ping_err2,ui->label_ping_err3};
    QLabel* timeval[3] = {ui->label_timeval1,ui->label_timeval2,ui->label_timeval3};
    QLabel* icmpseq[3] ={ui->label_icmpseq1,ui->label_icmpseq2,ui->label_icmpseq3};

    QStringList myList,message_List ;
//    qDebug() << strMsg;
    int len,i;

    if(ping_num < 0 || ping_num > 2)
        return;

    if(strMsg.endsWith("data.\n") || strMsg.startsWith("ping",Qt::CaseInsensitive)  || strMsg.contains("root", Qt::CaseInsensitive))
    {
        return;
    }

    if(strMsg.contains("timed out", Qt::CaseInsensitive))
    {
        error_count[ping_num] ++;
        ping_err[ping_num]->setText(QString::number(error_count[ping_num]));
        return;
    }

    message_List= strMsg.split('\n');
    len = message_List.length();
    for(i=0;i<len;i++)
    {
        if(message_List[i].contains("Host Unreachable", Qt::CaseInsensitive))
        {
            if(Ping_stat[ping_num]->text() != "异常")
            {
                Ping_stat[ping_num]->setText("异常");
                Ping_stat[ping_num]->setStyleSheet("QLabel{background-color:#ff0000;border-radius:5px;font: 20pt \"Ubuntu\";}");
            }

            if(myList[i].contains("Host Unreachable", Qt::CaseInsensitive))
                error_count[ping_num] ++;
            ping_err[ping_num]->setText(QString::number(error_count[ping_num]));
        }

        else
        {
            myList = message_List[i].split(' ');

            if(myList.length()>6)
            {
            //    qDebug()<< myList[6];    //time
                QStringList myList1 = myList[6].split('=');
                if(myList1.length()>1)
                {
                    timeval[ping_num]->setText(myList1[1]);
                    Ping_stat[ping_num]->setText("正常");
                    Ping_stat[ping_num]->setStyleSheet("QLabel{background-color:#00ff00;border-radius:5px;font: 20pt \"Ubuntu\";}");
                }
                else
                {
                    Ping_stat[ping_num]->setText("异常");
                    Ping_stat[ping_num]->setStyleSheet("QLabel{background-color:#ff0000;border-radius:5px;font: 20pt \"Ubuntu\";}");
                    error_count[ping_num] ++;
                    ping_err[ping_num]->setText(QString::number(error_count[0]));
                }
                myList1 = myList[4].split('=');   //icmp_seq
                if(myList1.length()>1)
                {
                    icmp_cur[ping_num] = myList1[1].toInt();
                    icmpseq[ping_num]->setText(myList1[1]);
                }
            }
        }
    }
}




void Widget::ping1_info_show()
{
    QString strMsg = myprocess_ping[0]->readAllStandardOutput();
    ping_info_show(strMsg,0);
}


void Widget::ping2_info_show()
{
    QString strMsg = myprocess_ping[1]->readAllStandardOutput();
    ping_info_show(strMsg,1);
}


void Widget::ping3_info_show()
{
    QString strMsg = myprocess_ping[2]->readAllStandardOutput();
    ping_info_show(strMsg,2);
}


//void Widget::ifconfig_errinfo_show()
//{
//    QString strMsg = myprocess_ping1->readAllStandardError();
//    qDebug() <<"error: "<< strMsg;
//}



//ping 进程结束
void Widget::ping1_finished_slot(int ret)
{
    Q_UNUSED(ret);
    if(myprocess_ping[0] != nullptr){
        delete myprocess_ping[0];
        myprocess_ping[0] = nullptr;
    }
}

void Widget::ping2_finished_slot(int ret)
{
    Q_UNUSED(ret);
    if(myprocess_ping[1] != nullptr){
        delete myprocess_ping[1];
        myprocess_ping[1] = nullptr;
    }
}


void Widget::ping3_finished_slot(int ret)
{
    Q_UNUSED(ret);
    if(myprocess_ping[2] != nullptr){
        delete myprocess_ping[2];
        myprocess_ping[2] = nullptr;
    }
}






//网络测试页 ： ping enp1s0f0
//void Widget::ping_pushButton_function(int ping_num)
//{
//    QString ping_str = "ping 192.168.0.200 -A ";
//    QPushButton* button[3] = {ui->pushButton_2,ui->pushButton_4,ui->pushButton_5};
//    QCheckBox* box[3] = {ui->checkBox_bigpack1,ui->checkBox_bigpack2,ui->checkBox_bigpack3};
//    ping_finished_slot_func_t pfunc[3] = {&Widget::ping1_finished_slot,&Widget::ping2_finished_slot,&Widget::ping3_finished_slot};
//    ping1_info_show_func_t show_func[3] = {&Widget::ping1_info_show,&Widget::ping2_info_show,&Widget::ping3_info_show};
//    QString str[3] = {"ping enp1s0f0","ping enp1s0f1","ping eth2"};

//    if(ping_num < 0 || ping_num > 2)
//        return;


//    if(button[ping_num]->text() == str[ping_num])
//    {
//        if(box[ping_num]->isChecked())
//        {
//            ping_str += " -s 65500 ";
//        }
//        myprocess_ping[ping_num] = new QProcess;
//        myprocess_ping[ping_num]->start(ping_str,QIODevice::ReadOnly);
//        button[ping_num]->setText("结束 ping");
//        box[ping_num]->setEnabled(false);
//        ping_status[ping_num] = true;
//        error_count[ping_num] = 0;
//        connect(this->myprocess_ping[ping_num], SIGNAL(readyReadStandardOutput()),this,SLOT(show_func[ping_num]()));//连接信号
////        connect(this->myprocess_ping1, SIGNAL(readyReadStandardError()),this,SLOT(ping1_errinfo_show()));//连接信号
//        connect(this->myprocess_ping[ping_num], SIGNAL(finished(int)),this,SLOT(pfunc[ping_num](int)));//连接信号
//    }
//    else
//    {
//        terminate_ping1();
//    }
//}



//网络测试页 ： ping enp1s0f0
void Widget::on_pushButton_2_clicked()
{
//    ping_pushButton_function(0);
    QString ping_str = "ping 192.168.0.200 ";
    if(ui->pushButton_2->text() == "ping enp1s0f0")
    {
        if(ui->checkBox_bigpack1->isChecked())
        {
            ping_str += " -s 65500 ";
        }
        if(ui->checkBox_adap1->isChecked())
        {
            ping_str += " -A ";
        }
        myprocess_ping[0] = new QProcess;
        myprocess_ping[0]->start(ping_str,QIODevice::ReadOnly);
        ui->pushButton_2->setText("结束 ping");
        ui->checkBox_bigpack1->setEnabled(false);
        ui->checkBox_adap1->setEnabled(false);
        ping_status[0] = true;
        error_count[0] = 0;
        ui->label_ping_err1->setText("0");
        connect(this->myprocess_ping[0], SIGNAL(readyReadStandardOutput()),this,SLOT(ping1_info_show()));//连接信号
//        connect(this->myprocess_ping1, SIGNAL(readyReadStandardError()),this,SLOT(ping1_errinfo_show()));//连接信号
        connect(this->myprocess_ping[0], SIGNAL(finished(int)),this,SLOT(ping1_finished_slot(int)));//连接信号
    }
    else
    {
        terminate_ping1();
    }
}


//网络测试页 ： ping enp1s0f1
void Widget::on_pushButton_4_clicked()
{
    QString ping_str = "ping 192.168.1.200 ";
    if(ui->pushButton_4->text() == "ping enp1s0f1")
    {
        if(ui->checkBox_bigpack2->isChecked())
        {
            ping_str += " -s 65500 ";
        }
        if(ui->checkBox_adap2->isChecked())
        {
            ping_str += " -A ";
        }
        myprocess_ping[1] = new QProcess;
        myprocess_ping[1]->start(ping_str,QIODevice::ReadOnly);
        ui->pushButton_4->setText("结束 ping");
        ui->checkBox_bigpack2->setEnabled(false);
        ui->checkBox_adap2->setEnabled(false);
        ping_status[1] = true;
        error_count[1] = 0;
        ui->label_ping_err2->setText("0");
        connect(this->myprocess_ping[1], SIGNAL(readyReadStandardOutput()),this,SLOT(ping2_info_show()));//连接信号
//        connect(this->myprocess_ping1, SIGNAL(readyReadStandardError()),this,SLOT(ping1_errinfo_show()));//连接信号
        connect(this->myprocess_ping[1], SIGNAL(finished(int)),this,SLOT(ping2_finished_slot(int)));//连接信号
    }
    else
    {
        terminate_ping2();
    }
}


//网络测试页 ： ping eth2
void Widget::on_pushButton_5_clicked()
{
    QString ping_str = "ping 192.168.2.200 ";
    if(ui->pushButton_5->text() == "ping eth2")
    {
        if(ui->checkBox_bigpack3->isChecked())
        {
            ping_str += " -s 65500 ";
        }
        if(ui->checkBox_adap3->isChecked())
        {
            ping_str += " -A ";
        }

        myprocess_ping[2] = new QProcess;
        myprocess_ping[2]->start(ping_str,QIODevice::ReadOnly);
        ui->pushButton_5->setText("结束 ping");
        ui->checkBox_bigpack3->setEnabled(false);
        ui->checkBox_adap3->setEnabled(false);
        ping_status[2] = true;
        error_count[2] = 0;
        ui->label_ping_err3->setText("0");
        connect(this->myprocess_ping[2], SIGNAL(readyReadStandardOutput()),this,SLOT(ping3_info_show()));//连接信号
//        connect(this->myprocess_ping1, SIGNAL(readyReadStandardError()),this,SLOT(ping1_errinfo_show()));//连接信号
        connect(this->myprocess_ping[2], SIGNAL(finished(int)),this,SLOT(ping3_finished_slot(int)));//连接信号
    }
    else
    {
        terminate_ping3();
    }
}


//音频测试页： 扬声器音量调整滑动

void Widget::on_horizontalScrollBar_SpeakVol_valueChanged(int value)
{
    qDebug()<<"扬声器音量 " << value  ;
#ifdef RK_3399_PLATFORM
    drvSetSpeakVolume(value);
#endif
}


//音频测试页： 手柄音量调整滑动
void Widget::on_horizontalScrollBar_HandVol_valueChanged(int value)
{
    qDebug()<<"手柄音量" << value  ;
#ifdef RK_3399_PLATFORM
    drvSetHandVolume(value);
#endif
}

//音频测试页： 耳机音量调整滑动
void Widget::on_horizontalScrollBar_EarphVol_valueChanged(int value)
{
    qDebug()<<"耳机音量" << value  ;
#ifdef RK_3399_PLATFORM
    drvSetEarphVolume(value);
#endif
}


//每一页的退出
void Widget::on_pushButton_Exit2_3_clicked()
{
    this->close();
}

void Widget::on_pushButton_Exit2_clicked()
{
    this->close();
}

void Widget::on_pushButton_Exit2_4_clicked()
{
    this->close();
}

void Widget::on_pushButton_Exit2_5_clicked()
{
    this->close();
}

void Widget::on_pushButton_Exit2_2_clicked()
{
    this->close();
}


//每一页的上一页
void Widget::on_pushButton_Last1_2_clicked()
{
    last_func_page_show();
}

void Widget::on_pushButton_Last1_5_clicked()
{
    last_func_page_show();
}

void Widget::on_pushButton_Last1_4_clicked()
{
    last_func_page_show();
}

void Widget::on_pushButton_Last1_3_clicked()
{
    ui->lineEdit_interval->clearFocus();
    last_func_page_show();

}


//每一页的下一页
void Widget::on_pushButton_Next2_3_clicked()
{
    ui->lineEdit_interval->clearFocus();
    next_func_page_show();
}

void Widget::on_pushButton_Next2_4_clicked()
{
    next_func_page_show();
}

void Widget::on_pushButton_Next2_5_clicked()
{
    next_func_page_show();
}


//触摸屏测试页的下一页
void Widget::on_pushButton_Next2_2_clicked()
{
    next_func_page_show();
}


//iicspi测试页的上一页
void Widget::on_pushButton_Last7_clicked()
{
    last_func_page_show();
}

//iicspi测试页的退出键
void Widget::on_pushButton_Exit7_clicked()
{
    this->close();
}




//键灯测试页：键灯亮度滑条调节
void Widget::on_verticalScrollBar_lightpwm2_valueChanged(int value)
{
    ui->label_light_value_2->setText(QString::number(value));
#ifdef RK_3399_PLATFORM
    drvSetLedBrt(value);
    lightpwm = value;
#endif

}


//键测试页:键灯关联
void Widget::on_checkBox_toggled(bool checked)
{
    key_light_connect = checked;
}



//网络测试页，配置3399网卡ip
void Widget::on_pushButton_3_clicked()
{
#ifdef RK_3399_PLATFORM
    int ret;
    char const *  str_name[] = {"enp1s0f0","enp1s0f1","eth2"};
    char num[] = {'0','1','2'};
    char const * str1_ipset = "nmcli connection modify --temporary '%s' connection.autoconnect yes ipv4.method manual ipv4.address 192.168.%c.%s/24 ipv4.gateway 192.168.%c.1  ipv4.dns 114.114.114.114";
    char cmd[256] = {0};

    if(ui->lineEdit_ip1->text().isEmpty())
        ui->lineEdit_ip1->setText("100");
    if(ui->lineEdit_ip2->text().isEmpty())
        ui->lineEdit_ip2->setText("100");
    if(ui->lineEdit_ip3->text().isEmpty())
        ui->lineEdit_ip3->setText("100");

    sprintf(cmd,str1_ipset,str_name[0],num[0],ui->lineEdit_ip1->text().toLatin1().data(),num[0]);
//    qDebug() << "cmd0 = " << cmd;
    ret = system(cmd);
    //ret = system("nmcli connection modify --temporary 'enp1s0f0' connection.autoconnect yes ipv4.method manual ipv4.address 192.168.0.100/24  ipv4.gateway 192.168.0.1  ipv4.dns 114.114.114.114");
//    qDebug() <<"system 1 :" << ret;
    ret = system("nmcli connection up enp1s0f0");
//    qDebug() <<"system 2 :" << ret;

    sprintf(cmd,str1_ipset,str_name[1],num[1],ui->lineEdit_ip2->text().toLatin1().data(),num[1]);
//    qDebug() << "cmd1 = " << cmd;
    ret = system(cmd);
    //ret = system("nmcli connection modify --temporary 'enp1s0f1' connection.autoconnect yes ipv4.method manual ipv4.address 192.168.1.100/24  ipv4.gateway 192.168.1.1  ipv4.dns 114.114.114.114");
//    qDebug() <<"system 3 :" << ret;
    ret = system("nmcli connection up enp1s0f1");
//    qDebug() <<"system 4 :" << ret;

    sprintf(cmd,str1_ipset,str_name[2],num[2],ui->lineEdit_ip3->text().toLatin1().data(),num[2]);
//    qDebug() << "cmd2 = " << cmd;
    ret = system(cmd);
    //ret = system("nmcli connection modify --temporary 'eth2' connection.autoconnect yes ipv4.method manual ipv4.address 192.168.2.100/24  ipv4.gateway 192.168.2.1  ipv4.dns 114.114.114.114");
//    qDebug() <<"system 5 :" << ret;
    if(ret)
        ret = system("nmcli con add type ethernet con-name eth2 ifname eth2 ip4 192.168.2.100/24 gw4 192.168.2.1");
    else
        ret = system("nmcli connection up eth2");
//    qDebug() <<"system 6 :" << ret;
#endif

    getNetDeviceStats();
}


void Widget::ifconfig_info_show(int ret)
{
    if(ret == 0)
    {
        ui->textBrowser_ifconfig->setText(myprocess_ifconfig->readAllStandardOutput());
        ui->textBrowser_ifconfig->setVisible(true);
        ui->textBrowser_ifconfig->setFocus();
        ui->textBrowser_ifconfig->raise();
    }
}



void Widget::on_pushButton_ifconfig_clicked()
{
    if(ui->pushButton_ifconfig->text() == "查看rk3399主板IP")
    {

        myprocess_ifconfig->start("ifconfig");
        connect(this->myprocess_ifconfig, SIGNAL(finished(int)),this,SLOT(ifconfig_info_show(int)));//连接信号
        ui->pushButton_ifconfig->setText("关闭查看rk3399IP");
    }
    else if(ui->pushButton_ifconfig->text() == "关闭查看rk3399IP")
    {
        ui->textBrowser_ifconfig->setVisible(false);
        ui->pushButton_ifconfig->setText("查看rk3399主板IP");
//        delete myprocess_ifconfig;
    }

}

void Widget::on_radioButton_loop_toggled(bool checked)
{
    if(checked)
    {
        ui->radioButton_michand->setEnabled(true);
        ui->radioButton_micpanel->setEnabled(true);
    }
}

void Widget::on_radioButton_playmusic_toggled(bool checked)
{
    if(checked)
    {
        ui->radioButton_michand->setEnabled(false);
        ui->radioButton_micpanel->setEnabled(false);
    }
}

void Widget::on_radioButton_playrec_toggled(bool checked)
{
    if(checked)
    {
        ui->radioButton_michand->setEnabled(false);
        ui->radioButton_micpanel->setEnabled(false);
    }
}

void Widget::on_radioButton_rec_toggled(bool checked)
{
    if(checked)
    {
        ui->radioButton_michand->setEnabled(true);
        ui->radioButton_micpanel->setEnabled(true);
    }
}





void Widget::on_horizontalScrollBar_light_valueChanged(int value)
{
    ui->label_light_val->setText(QString::number(value));
#ifdef RK_3399_PLATFORM
    drvSetLcdBrt(value);
#endif
}




void Widget::iicspi_info_show(int ret)
{
    if(ret == 0)
    {
        ui->textBrowser_IICSPI->setText(myprocess_iicspi->readAllStandardOutput());
//        ui->textBrowser_IICSPI->setVisible(true);
        ui->textBrowser_IICSPI->setFocus();
//        ui->textBrowser_IICSPI->raise();
    }
}



//IIC、SPI数据读取
void Widget::on_pushButton_8_clicked()
{
    if(iicspi_connect == 0)
    {
        iicspi_connect = 1;
        connect(this->myprocess_iicspi, SIGNAL(finished(int)),this,SLOT(iicspi_info_show(int)));//连接信号
    }
    if(ui->radioButton_IICtest->isChecked())
    {
        myprocess_iicspi->start("i2cdump -f -y 4 0x50");

    }
    else if(ui->radioButton_Spitest->isChecked())
    {
        myprocess_iicspi->start("/home/deepin/gd25qxx_test r");
    }
}



//IIC、SPII数据写入
void Widget::on_pushButton_9_clicked()
{
    char cmd[128];
    unsigned char i;

    if(iicspi_connect == 1)
    {
        iicspi_connect = 0;
        disconnect(this->myprocess_iicspi, SIGNAL(finished(int)),this,SLOT(iicspi_info_show(int)));//连接信号
    }
    if(ui->radioButton_IICtest->isChecked())
    {

        for(i=0;i<128;i++)
        {
            sprintf(cmd,"%s %d %d","i2cset -f -y 4 0x50",i,i+1);
            myprocess_iicspi->start(cmd);
            myprocess_iicspi->waitForFinished();
        }
    }
    else if(ui->radioButton_Spitest->isChecked())
    {
        myprocess_iicspi->start("/home/deepin/gd25qxx_test w");
        myprocess_iicspi->waitForFinished();
    }
}




//IIC、SPI数据擦除
void Widget::on_pushButton_10_clicked()
{
    char cmd[128];
    unsigned char i;

    if(iicspi_connect == 1)
    {
        iicspi_connect = 0;
        disconnect(this->myprocess_iicspi, SIGNAL(finished(int)),this,SLOT(iicspi_info_show(int)));//连接信号
    }
    if(ui->radioButton_IICtest->isChecked())
    {

        for(i=0;i<128;i++)
        {
            sprintf(cmd,"%s %d %d","i2cset -f -y 4 0x50",i,255);
            myprocess_iicspi->start(cmd);
            myprocess_iicspi->waitForFinished();
        }
    }
    else if(ui->radioButton_Spitest->isChecked())
    {
        myprocess_iicspi->start("/home/deepin/gd25qxx_test e");
        myprocess_iicspi->waitForFinished();
    }
}

void Widget::on_radioButton_micpanel_clicked()
{
//    char cmd[128] = "i2cset -f -y 4 0x10 0xa 0";
    myprocess_iicspi->start("i2cset -f -y 4 0x10 0xa 0");
    myprocess_iicspi->waitForFinished();
}

void Widget::on_radioButton_michand_clicked()
{
//    char cmd[128] = "i2cset -f -y 4 0x10 0xa 0x50";
    myprocess_iicspi->start("i2cset -f -y 4 0x10 0xa 0x50");
    myprocess_iicspi->waitForFinished();
}



void Widget::on_radioButton_SpeakVol_toggled(bool checked)
{
    if(checked)
    {
#ifdef RK_3399_PLATFORM
        drvDisableSpeaker();
#endif
    }
    else
    {
#ifdef RK_3399_PLATFORM
        drvEnableSpeaker();
#endif
    }
}

void Widget::on_radioButton_HandVol_toggled(bool checked)
{
    if(checked)
    {
#ifdef RK_3399_PLATFORM
        drvDisableHandout();
#endif
    }
    else
    {
#ifdef RK_3399_PLATFORM
        drvEnableHandout();
#endif
    }
}

void Widget::on_pushButton_Last7_2_clicked()
{
    last_func_page_show();
}

void Widget::on_pushButton_Exit7_2_clicked()
{
    this->close();
}

void Widget::on_pushButton_Next7_clicked()
{
    next_func_page_show();
}



void Widget::on_pushButton_start_cpustress_clicked()
{
    QString cmd;
    int cpu_n,mem_n;

    cpu_n = ui->comboBox_cpu->currentText().toInt();
    mem_n = ui->comboBox_memory->currentText().toInt();

    if(ui->checkBox_cpu_n->isChecked() && ui->checkBox_mem_n->isChecked())
    {
        if(cpu_n > 1)
            cmd = "stress-ng -c " + QString::number(cpu_n-1) + " --vm 1  --vm-bytes "+ QString::number(mem_n) + "% --vm-method all --verify -t 100d";
        else
            cmd = "stress-ng --vm 1  --vm-bytes "+ QString::number(mem_n) + "% --vm-method all --verify -t 100d";
    }
    else if(ui->checkBox_cpu_n->isChecked())
    {
        cmd =  "stress-ng -c " + QString::number(cpu_n) +" -t 100d";
    }
    else if(ui->checkBox_mem_n->isChecked())
    {
        cmd = "stress-ng --vm 1 --vm-bytes "+ QString::number(mem_n) +"% --vm-method all --verify -t 100d";
    }
    else
        return;
    //qDebug() << "cmd" << cmd;

    if(ui->pushButton_start_cpustress->text() == "开始压力测试")
    {
        myprocess_cpu_stress->start(cmd);
        ui->pushButton_start_cpustress->setText("结束压力测试");
        ui->pushButton_start_cpustress->setStyleSheet("QPushButton{background-color:#ff0000;font: 20pt \"Ubuntu\";}");
    }
    else
    {
        myprocess_cpu_stress->kill();
        myprocess_cpu_stress->waitForFinished();
        ui->pushButton_start_cpustress->setText("开始压力测试");
        ui->pushButton_start_cpustress->setStyleSheet("QPushButton{background-color:#00ff00;font: 20pt \"Ubuntu\";}");
    }
}

//struct system_config
//{
//    int is_cpu_stress_start;   //启动开始cpu压力测试？0表示不开启，1开启测试
//    int is_gpio_flow_start;   //启动开启gpio流水灯吗？
//    int is_key_lights_start;  //启动开启键灯吗？
//    int default_show_page;    //启动默认显示页面,默认是第一页
//    int cpu_test_core_num;    //cpu的测试核心数（第8位表示是否勾上）
//    int mem_test_usage;       //内存测试的百分比（第8位表示是否勾上）
//}g_sys_conf;


//开机自动启动配置，记录到文件中
void Widget::on_checkBox_cpu_stress_toggled(bool checked)
{
    g_sys_conf.is_cpu_stress_start = checked;
    SetConfigFile();
}


//开机自动启动配置，记录到文件中
void Widget::on_checkBox_gpio_flow_toggled(bool checked)
{
    g_sys_conf.is_gpio_flow_start = checked;
    SetConfigFile();
}


//开机自动启动配置，记录到文件中
void Widget::on_checkBox_keyLights_toggled(bool checked)
{
    g_sys_conf.is_key_lights_start = checked;
    SetConfigFile();
}



void Widget::on_comboBox_memory_currentIndexChanged(int index)
{
    g_sys_conf.mem_test_usage = index;
    SetConfigFile();
}

void Widget::on_comboBox_cpu_currentIndexChanged(int index)
{
    g_sys_conf.cpu_test_core_num = index;
    SetConfigFile();
}






void Widget::on_checkBox_cpu_n_toggled(bool checked)
{
    g_sys_conf.is_cpu_test_checked = checked;
    SetConfigFile();
}

void Widget::on_checkBox_mem_n_toggled(bool checked)
{
    g_sys_conf.is_mem_test_checked = checked;
    SetConfigFile();
}

void Widget::on_comboBox_currentIndexChanged(int index)
{
    g_sys_conf.default_show_page = index;
    SetConfigFile();
}
