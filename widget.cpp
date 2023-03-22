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
#ifdef  __cplusplus
}
#endif
#endif


const char* g_build_time_str = "Buildtime :" __DATE__ " " __TIME__ ;   //获得编译时间

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define PAGES 6  //显示总页数



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




Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);
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
    connect ( timer_net_stat, SIGNAL ( timeout() ), this, SLOT ( timer_net_stat_slot_Function() ) );

    ui->label_color->setVisible(false);
    //ui->label_color->installEventFilter(this);
    this->installEventFilter(this);
//    ui->lineEdit_interval->installEventFilter(this);


    page2_show_color = 0;
    is_test_press = 0;     //测试键没有按下

    key_light_connect = 1;  //键灯关联

    ui->stackedWidget->setCurrentIndex(0);


    /* 一定要先设置groove，不然handle的很多效果将没有*/
    QString sliderstyle = QString("QSlider::groove:horizontal{border:none; height:22px; border-radius:5px; background:rgba(200, 19, 39, 100);}"
                                  /* 上下边距和左右边距*/
                                  "QSlider::handle:horizontal{border:none; margin:-5px 0px; width:22px; height:22px; border-radius:5px;"
                                                             "background:rgba(255, 255, 0, 250);}"
                                 /*划过部分*/
                                 "QSlider::sub-page:horizontal{background:rgba(255, 19, 39, 100); border-radius:5px;}"
                                 /*未划过部分*/
                                 "QSlider::add-page:horizontal{background:rgba(0, 255, 0, 100); border-radius:5px;}");
    //        ui->horizontalSlider->setStyleSheet(sliderstyle);
    ui->horizontalSlider_EarphVol->setStyleSheet(sliderstyle);
    ui->horizontalSlider_HandVol->setStyleSheet(sliderstyle);
    ui->horizontalSlider_SpeakVol->setStyleSheet(sliderstyle);
    ui->horizontalSlider_4->setStyleSheet(sliderstyle);

    intValidator = new QIntValidator;
    intValidator->setRange(1,999999);
    ui->lineEdit_interval->setValidator(intValidator);
//    intValidator_ip = new QIntValidator;
//    intValidator_ip->setRange(1,254);

    QRegExp rx("^([1-9]|[1-9]\\d|(1[0-9]\\d)|(2[0-4]\\d)|(2[0-5][0-4]))$");//输入范围为【1-254】
    pReg = new QRegExpValidator(rx, this);
    ui->lineEdit_ip1->setValidator(pReg);
    ui->lineEdit_ip2->setValidator(pReg);
    ui->lineEdit_ip3->setValidator(pReg);


    ui->textBrowser_ifconfig->setVisible(false);//显示ip信息的暂时不可见


//    ui->lineEdit_interval->setPlaceholderText("请输入1-999999");
//    ui->lineEdit_interval->inputMask();

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

    icmp_cur[0] = 0;
    icmp_cur[1] = 0;
    icmp_cur[2] = 0;
    icmp_saved[0] = 0;
    icmp_saved[1] = 0;
    icmp_saved[2] = 0;

    ui->label_ping_reson1->setText("");
    ui->label_ping_reson2->setText("");
    ui->label_ping_reson3->setText("");

}






Widget::~Widget()
{
#ifdef RK_3399_PLATFORM
    drvCoreBoardExit();
    timer_key_leds->stop();
    delete timer_key_leds;
#endif
    delete intValidator;
    delete pReg;
    delete ui;
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

        if(ui->stackedWidget->currentIndex() == 2)//lcd颜色测试页，按键变换颜色
        {
            if(KeyEvent->key() == Qt::Key_Up)
            {
                last_color_page_show();
            }
            else if(KeyEvent->key() == Qt::Key_Down)
            {
                next_color_page_show();
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
                ui->verticalSlider_lightpwm2->setValue(lightpwm);

            }
            else if(KeyEvent->key() == Qt::Key_Down)
            {
                if(lightpwm > 0){
                    lightpwm -=5;
                    if(lightpwm < 0 )
                        lightpwm = 0;
                 }
                ui->verticalSlider_lightpwm2->setValue(lightpwm);//drvSetLedBrt(lightpwm);
            }
        }
        else if(ui->stackedWidget->currentIndex() == 0) //第一页，按键测试页
        {
            Palette_button(1,KeyEvent->key());
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
                on_pushButton_2_clicked();
            }
            else if(KeyEvent->key() == Qt::Key_F7)
            {
                on_pushButton_4_clicked();
            }
            else if(KeyEvent->key() == Qt::Key_F9)
            {
                on_pushButton_5_clicked();
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
    ui->pushButton_start_color_test->setVisible(false);
    ui->pushButton_Exit2->setVisible(false);
    ui->pushButton_Last1->setVisible(false);
    ui->pushButton_Next2->setVisible(false);
    ui->label_Page2_title->setVisible(false);
    ui->label_color->setVisible(true);
    ui->label_4->setVisible(false);
    ui->label_10->setVisible(false);
    ui->label_11->setVisible(false);
    ui->label_12->setVisible(false);
    ui->textBrowser->setVisible(false);
    ui->label_23->setVisible(false);
//     timer_color->start(2000);  //2s
     ui->label_color->setStyleSheet("background-color:rgb(255,0,0)");
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
        ui->pushButton_start_color_test->setVisible(true);
        ui->pushButton_Exit2->setVisible(true);
        ui->pushButton_Last1->setVisible(true);
        ui->pushButton_Next2->setVisible(true);
        ui->label_Page2_title->setVisible(true);
        ui->label_color->setVisible(false);
        ui->label_4->setVisible(true);
        ui->label_10->setVisible(true);
        ui->label_11->setVisible(true);
        ui->label_12->setVisible(true);
        ui->textBrowser->setVisible(true);
        ui->label_23->setVisible(true);
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
    if(ui->pushButton->text() == "键灯全部点亮"){
        ui->pushButton->setText("键灯全部熄灭");
        ui->lineEdit_interval->clearFocus();
#ifdef RK_3399_PLATFORM
       drvLightAllLED();
#endif
    }
    else{
        ui->lineEdit_interval->clearFocus();
        ui->pushButton->setText("键灯全部点亮");
#ifdef RK_3399_PLATFORM
       drvDimAllLED();
#endif
    }
}


//开始触摸测试
void Widget::on_pushButton_start_lcd_touch_clicked()
{
    lcd_touch_ui = new fingerpaint(this);
    lcd_touch_ui->show();
}



//音频测试页：播放按键
void Widget::on_pushButton_Play_clicked()
{
    if(ui->pushButton_Play->text() == "开始"){
        if(ui->radioButton_rec->isChecked())
        {
            qDebug()  << "录音测试" ;
        }
        else if(ui->radioButton_loop->isChecked())
        {
            qDebug()  << "回环测试" ;
        }
        else if(ui->radioButton_playrec->isChecked())
        {
            qDebug()  << "播放录音" ;
        }
        else if(ui->radioButton_playmusic->isChecked())
        {
            qDebug()  << "播放音乐" ;
        }
        ui->radioButton_rec->setEnabled(false);
        ui->radioButton_loop->setEnabled(false);
        ui->radioButton_playrec->setEnabled(false);
        ui->radioButton_playmusic->setEnabled(false);
        ui->pushButton_Play->setText("结束");
    }
    else{
        ui->radioButton_rec->setEnabled(true);
        ui->radioButton_loop->setEnabled(true);
        ui->radioButton_playrec->setEnabled(true);
        ui->radioButton_playmusic->setEnabled(true);
        ui->pushButton_Play->setText("开始");
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
                {
                    ui->pushButton_2->setEnabled(false);
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
void Widget::on_horizontalSlider_SpeakVol_valueChanged(int value)
{
    qDebug()<<"扬声器音量 " << value  ;
}


//音频测试页： 手柄音量调整滑动
void Widget::on_horizontalSlider_HandVol_valueChanged(int value)
{
    qDebug()<<"手柄音量" << value  ;
}

//音频测试页： 耳机音量调整滑动
void Widget::on_horizontalSlider_EarphVol_valueChanged(int value)
{
    qDebug()<<"耳机音量" << value  ;
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

//键灯测试页：键灯亮度滑条调节
void Widget::on_verticalSlider_lightpwm2_valueChanged(int value)
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
    }
}



void Widget::on_pushButton_ifconfig_clicked()
{
    if(ui->pushButton_ifconfig->text() == "查看rk3399主板IP")
    {
        myprocess_ifconfig = new QProcess;
        myprocess_ifconfig->start("ifconfig");
        connect(this->myprocess_ifconfig, SIGNAL(finished(int)),this,SLOT(ifconfig_info_show(int)));//连接信号
        ui->pushButton_ifconfig->setText("关闭查看rk3399IP");
    }
    else if(ui->pushButton_ifconfig->text() == "关闭查看rk3399IP")
    {
        ui->textBrowser_ifconfig->setVisible(false);
        ui->pushButton_ifconfig->setText("查看rk3399主板IP");
        delete myprocess_ifconfig;
    }

}
