//
//  IRenderer.h
//  TNG

#ifndef TNG_NETRENDERERCLIENT_h
#define TNG_NETRENDERERCLIENT_h

#include <renderer/IRenderer.h>

#include "NetCommands.h"


//NETWORKING
#include <base/Error.h>
#include <base/BytePacket.h>
#include <net/TCPNetworkAddress.h>
#include <net/LoopbackNetworkService.h>
#include <net/LoopbackConnectionListener.h>
#include <net/TCPNetworkService.h>

//COMPRESSION
#include <lz4/lz4.h>
#include <lz4/lz4hc.h>
#include <mutex>

using namespace mocca;
using namespace mocca::net;


namespace Tuvok{
  namespace Renderer {

    class NetRendererClient : public IRenderer{
    public:
        NetRendererClient(std::string ip,
                          int port,
                          Visibility visibility,
                          Core::Math::Vec2ui resolution,
                          std::string dataset,
                          std::string tf);
        ~NetRendererClient();

		// THREADING
		void startRenderThread(){};
		void pauseRenderThread(){};
		void stopRenderThread(){};

        // BASIC INTERACTION METHODS

        //Camera Controls
        void SetFirstPersonMode(bool mode){};
        void RotateCamera(Core::Math::Vec3f rotation);                                              //
        void MoveCamera(Core::Math::Vec3f direction);                                               //
        void SetCamera(Camera c){};
        void SetCameraZoom(float f);                                                                //

        //read the last framebuffer
        void ReadFrameBuffer(std::vector<uint8_t>& pixels, int& width, int& height, int& componentCount){}; //LEGACY
        FrameData ReadFrameBuffer(); //
        // END BASIC INTERACTION METHODS


		//ISO FUNCTIONS
		void SetIsoValue(float fIsoValue);                                                          //
		virtual void SetIsoValueRelative(float fIsovalue){};
		float GetIsoValue();                                                                        //
		virtual void SetIsosurfaceColor(const Core::Math::Vec3f& vColor) {};
		virtual Core::Math::Vec3f GetIsosurfaceColor() const { return Core::Math::Vec3f(); };
		virtual void SetColorDataset(bool isColor) {};
		//END ISO FUNCTIONS


        virtual void SetClearFrameBuffer(bool bClearFramebuffer){};
		virtual bool GetClearFrameBuffer() const  { return true; };

        virtual void SetViewParameters(float angle, float znear, float zfar) {};
		virtual Core::Math::Mat4f& GetProjectionMatrix() { Core::Math::Mat4f m; return m; };
		virtual Core::Math::Mat4f& GetViewMatrix() { Core::Math::Mat4f m; return m;  };

        virtual bool SetBackgroundColors(Core::Math::Vec3f vTopColor,
			Core::Math::Vec3f vBottomColor) {
			return true;
		};
		virtual Core::Math::Vec3f GetBackgroundColor(const uint8_t index) { return Core::Math::Vec3f(); };

        virtual void SetUseLighting(bool bUseLighting) {};
		virtual void SetLightingParameters(Core::Math::Vec4f cAmbient,
			Core::Math::Vec4f cDiffuse,
			Core::Math::Vec4f cSpecular
			) {};
		virtual const Core::Math::Vec4f GetAmbientColor() { return Core::Math::Vec4f(); };
		virtual const Core::Math::Vec4f GetDiffuseColor() { return Core::Math::Vec4f(); };
		virtual const Core::Math::Vec4f GetSpecularColor() { return Core::Math::Vec4f(); };
        virtual void SetSampleRateModifier(float fSampleRateModifier) {};
		virtual void ResetCamera() {};

		virtual Core::Math::Vec2ui GetSize() const { return Core::Math::Vec2ui(); };

        virtual void Resize(const Core::Math::Vec2ui& vWinSize) {};

        virtual void GetVolumeAABB(Core::Math::Vec3f& vCenter, Core::Math::Vec3f& vExtend) const {};

        //implement in subclasses
        virtual void SetViewPort(Core::Math::Vec2ui lower_left, Core::Math::Vec2ui upper_right,
                                 bool decrease_screen_res = false)  {}; //glrenderer

	    virtual void SetLoDFactor(const float f) {};

		virtual float GetSampleRateModifier() { return 0.0f; };

	    virtual void ClipVolume(Core::Math::Vec3f minValues, Core::Math::Vec3f maxValues) {};

	    virtual void SetCompositeMode(ECompositeDisplay displaymode) {};

	    virtual void SetRenderMode(ERenderMode mode) {};

	    virtual void Set1DTransferFunction(std::vector<Core::Math::Vec4f> data) {};

		virtual Core::Math::Vec4f readVolumePosition(Core::Math::Vec2ui v) { return Core::Math::Vec4f(); };

	    virtual void setClearViewEnabled(bool b) {};

	    virtual void setClearViewRadius(float f) {};

		virtual State getRenderState() { return State(); };

	    virtual void setRenderState(State renderState) {};

	    virtual void SwitchPagingStrategy(MissingBrickStrategy brickStrategy){};

	    virtual void run(){};

        //PROPABLY PROTECTED METHODS // HAVE TO CHECK
        virtual void SetDataset(Tuvok::IOPtr dio) {};

        void stopRenderer(){};

        //network methods
        private:
        void connectToServer(std::string ip, int port);
        void openTicket();
        void initializeRenderer(Visibility visibility,
                                Core::Math::Vec2ui resolution,
                                std::string dataset,
                                std::string tf);

        void sendString(std::string msg);

        //member
        private:

        std::unique_ptr<AbstractConnection> connection;
        uint64_t                            _framebuffersize;
        FrameData                           _currentFrame;

        int32_t                             _iTicketID;
        uint32_t                            _iCallID;

        std::mutex                          _contextMutex;
		std::string							_ip;

    };
  };
};

#endif