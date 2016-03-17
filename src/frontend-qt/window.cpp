#include "window.h"

#include "ui_window.h"
#include <QTimer>

#include <iostream>

#include "mocca/log/LogManager.h"

#include "commands/Vcl.h"

using namespace std;
using namespace mocca::net;

static int reconnectInSec = 5;

Window::Window(QWidget* parent)
: QMainWindow(parent)
, _initDone(false)
, _renderWidth(800)
, _renderHeight(800)
, ui(new Ui::Window) {
    ui->setupUi(this);
    LINFO("Window created");
    
    QTimer* timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(10);
}

Window::~Window() {
    delete ui;
}

/* OLD
 void Window::initRenderer() {
 trinity::StreamingParams params(_renderWidth, _renderHeight);
 
 Endpoint endpointIO(ConnectionFactorySelector::tcpPrefixed(), "localhost", "6678");
 
 // the file id will be available after implementing the listdata command
 int fileId = 0;
 _renderer = _processingNode->initRenderer(trinity::VclType::GridLeaper, fileId, endpointIO, params);
 
 // sending commands
 _renderer->initContext();
 std::this_thread::sleep_for(std::chrono::seconds(1));
 _initDone = true;
 }
 
 void Window::on_IOconnectIP_clicked() {
 Endpoint endpointIO(ConnectionFactorySelector::tcpPrefixed(), "localhost", "6678");
 
 _ioNode = std::unique_ptr<trinity::IONodeProxy>(new trinity::IONodeProxy(endpointIO));
 }
 
 void Window::on_PRconnectIP_clicked() {
 Endpoint endpoint(ConnectionFactorySelector::tcpPrefixed(), "localhost", "8678");
 
 _processingNode = std::unique_ptr<trinity::ProcessingNodeProxy>(new trinity::ProcessingNodeProxy(endpoint));
 LINFO("connected to processing node");
 
 initRenderer();
 }
 
 static float rot = 0.0f;
 void Window::update() {
 if (_initDone) {
 _renderer->setIsoValue(rot);
 rot += 0.01f;
 
 auto frameNullable = _renderer->getVisStream()->get();
 if (!frameNullable.isNull()) {
 auto frame = frameNullable.release();
 ui->openGLWidget->setData(_renderWidth, _renderHeight, frame.data());
 ui->openGLWidget->repaint();
 }
 }
 }
*/


void Window::initRenderer() {
    trinity::StreamingParams params(_renderWidth, _renderHeight);
 
    Endpoint endpointIO(ConnectionFactorySelector::tcpPrefixed(), "localhost", "6678");
    
    // the file id will be available after implementing the listdata command
    int fileId = 0;
    _renderer = _processingNode->initRenderer(trinity::VclType::GridLeaper, fileId, endpointIO, params);
    
    // sending commands
    _renderer->initContext();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    _initDone = true;
}

void Window::on_IOconnectIP_clicked() {
    Endpoint endpointIO(ConnectionFactorySelector::tcpPrefixed(),
                        ui->IOaddressIPedit->text().toUtf8().constData(),
                        ui->IOaddressPortedit->text().toUtf8().constData());
    _ioNode = std::unique_ptr<trinity::IONodeProxy>(new trinity::IONodeProxy(endpointIO));
}

void Window::on_PRconnectIP_clicked() {
    Endpoint endpoint(ConnectionFactorySelector::tcpPrefixed(),
                      ui->PRaddressIPedit->text().toUtf8().constData(),
                      ui->PRaddressPortedit->text().toUtf8().constData());
    
    _processingNode = std::unique_ptr<trinity::ProcessingNodeProxy>(new trinity::ProcessingNodeProxy(endpoint));
    LINFO("connected to processing node");
    
    initRenderer();
}

static float rot = 0.0f;
void Window::update() {
    if (_initDone) {
        _renderer->setIsoValue(rot);
        rot += 0.01f;
        
        auto frameNullable = _renderer->getVisStream()->get();
        if (!frameNullable.isNull()) {
            auto frame = frameNullable.release();
            ui->openGLWidget->setData(_renderWidth, _renderHeight, frame.data());
            ui->openGLWidget->repaint();
        }
    }
}

void Window::repaint() {}
