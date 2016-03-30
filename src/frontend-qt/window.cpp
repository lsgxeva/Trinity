#include "window.h"

#include "ui_window.h"
#include <QMouseEvent>
#include <QTimer>
#include <QWheelEvent>

#include <iostream>

#include "mocca/log/LogManager.h"

#include "commands/Vcl.h"
#include "common/TrinityError.h"

using namespace std;
using namespace mocca::net;

static int reconnectInSec = 5;

Window::Window(QWidget* parent)
: QMainWindow(parent)
, _initDone(false)
, ui(new Ui::Window) {
  ui->setupUi(this);
  LINFO("Window created");
  QTimer* timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(update()));
  timer->start(10);
  ui->dsWidget->topLevelItem(0)->setSelected(true);
}

Window::~Window() {
  delete ui;
}

void Window::printDataTree(const std::string& dataId, const std::string& indent) const {
  try {
    if (indent.empty()) {
      LINFO("Listing " + dataId);
    }
    
    std::vector<trinity::IOData> list = _ioNode->listFiles(dataId);
    for (auto e : list) {
      if (e.getDataType() == trinity::IOData::DataType::Directory) {
        LINFO(indent + "Directory:" + e.getName() + "\t (" + e.getFileId() + ")");
        printDataTree(e.getFileId(), indent + "  ");
      } else {
        LINFO(indent + e.getName() + "\t (" + e.getFileId() + ")");
      }
    }
  } catch (const trinity::TrinityError&) {
  }
}

void Window::initRenderer() {
  _renderWidth = (ui->resx->text().toInt());
  _renderHeight = (ui->resy->text().toInt());
  trinity::StreamingParams params(_renderWidth, _renderHeight);
  
  Endpoint endpointIO(ConnectionFactorySelector::tcpPrefixed(), ui->IOaddressIPedit->text().toStdString(),
                      ui->IOaddressPortedit->text().toStdString());
  
  // the file id will be available after implementing the listdata command
  // std::string fileId = "FractalData@3a"; // fixme: should be selectable from the gui
  // std::string fileId = "UVFData@c60.uvf"; // fixme: should be selectable from the gui

  std::string fileId = (ui->dsWidget->currentItem()) ? ui->dsWidget->currentItem()->text(0).toStdString() : "FractalData@3a";
  
  printDataTree("FractalData");
  printDataTree("UVFData");
  
  try {
    m_renderer = _processingNode->initRenderer(trinity::VclType::SimpleRenderer, fileId, endpointIO, params);
  } catch (const trinity::TrinityError&) {
    LERROR("(qt) no connection to a renderer");
  }
  
  // set initial parameters here
  
  // TODO
  
  // initial parameter end
  
  m_renderer->startRendering();
  
  std::this_thread::sleep_for(std::chrono::seconds(1));
  _initDone = true;
}

void Window::on_IOconnectIP_clicked() {
  Endpoint endpointIO(ConnectionFactorySelector::tcpPrefixed(), ui->IOaddressIPedit->text().toStdString(),
                      ui->IOaddressPortedit->text().toStdString());
  _ioNode = std::unique_ptr<trinity::IONodeProxy>(new trinity::IONodeProxy(endpointIO));
}

void Window::on_PRconnectIP_clicked() {
  Endpoint endpoint(ConnectionFactorySelector::tcpPrefixed(), ui->PRaddressIPedit->text().toStdString(),
                    ui->PRaddressPortedit->text().toStdString());
  
  _processingNode = std::unique_ptr<trinity::ProcessingNodeProxy>(new trinity::ProcessingNodeProxy(endpoint));
  LINFO("connected to processing node");
  initRenderer();
}

void Window::update() {
  if (_initDone && m_renderer) {
    auto frameNullable = m_renderer->getVisStream()->get();
    if (!frameNullable.isNull()) {
      auto frame = frameNullable.release();
      ui->openGLWidget->setData(_renderWidth, _renderHeight, frame.data());
      ui->openGLWidget->repaint();
    }
    m_renderer->proceedRendering();
  }
}

void Window::repaint() {}

void Window::wheelEvent(QWheelEvent* event) {
  if (!_initDone)
    return;
  
  static float isoValue = 0;
  isoValue += event->delta() / 2400.f;
  m_renderer->setIsoValue(0, isoValue);
}

void Window::mousePressEvent(QMouseEvent* event) {
  m_mousePosX = event->localPos().x();
  m_mousePosY = event->localPos().y();
}

void Window::mouseMoveEvent(QMouseEvent* event) {
  if (!_initDone)
    return;
  
  float deltaX = (m_mousePosX - event->localPos().x()) / 200.f;
  float deltaY = (m_mousePosY - event->localPos().y()) / 200.f;
  if (event->buttons() & Qt::RightButton) {
    Core::Math::Vec3f vec(-deltaX, deltaY, .0f);
    m_renderer->moveCamera(vec);
  } else if (event->buttons() & Qt::LeftButton) {
    Core::Math::Vec3f vec(-deltaY, -deltaX, .0f);
    m_renderer->rotateScene(vec);
  }
  m_mousePosX = event->localPos().x();
  m_mousePosY = event->localPos().y();
}