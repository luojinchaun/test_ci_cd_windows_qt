#include <QApplication>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QWidget window;
    window.resize(200, 100);
    window.move(300, 300);
    window.setWindowTitle("Simple Qt Window");
    window.show();
    return app.exec();
}
