#ifndef FINGERPAINT_H
#define FINGERPAINT_H

#include <QMainWindow>

#include <QList>
#include <QMainWindow>

class ScribbleArea;



class fingerpaint : public QMainWindow
{
    Q_OBJECT
public:
    explicit fingerpaint(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void open();
    void save();
    void about();

private:
    void createActions();
    void createMenus();
    bool maybeSave();
    bool saveFile(const QByteArray &fileFormat);

    ScribbleArea *scribbleArea;

    QMenu *saveAsMenu;
    QMenu *fileMenu;
    QMenu *optionMenu;
    QMenu *helpMenu;

    QAction *openAct;
    QList<QAction *> saveAsActs;
    QAction *exitAct;
    QAction *printAct;
    QAction *clearScreenAct;
    QAction *aboutAct;
    QAction *aboutQtAct;

public slots:
};

#endif // FINGERPAINT_H
