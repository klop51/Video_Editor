#include <QApplication>
#include "player_window.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if(argc < 2) {
        return 1;
    }
    PlayerWindow w(argv[1]);
    w.resize(640,360);
    w.show();
    return app.exec();
}
