
#include "pylon_camera.h"
#include <yat/plugin/PlugInManager.h>
#include <yat/threading/Condition.h>
#include <baslerpylon/IGrabber.h>
#include <iostream>

namespace {

  GrabAPI::IGrabber* pGrabber = 0;
  //yat::Mutex* pImageMutex = NULL;
  //yat::Condition* pImageCondition = NULL;
  //yat::Mutex pImageMutex;
  //yat::Condition pImageCondition(pImageMutex);
  guint imageCounter = 0;
  guint currentImage = 0;
  GrabAPI::Image* image = NULL;

  void handle_image(GrabAPI::Image* newImage)
  {
    //g_assert(pImageMutex);
    //g_assert(pImageCondition);
    //yat::MutexLock lock(pImageMutex);
    delete image;
    image = newImage;
    imageCounter++;
    std::cerr << "signal next image ready " << std::endl;
    //pImageCondition.signal();
  }

  void yat_exception_to_gerror(const yat::Exception& e, GError** error)
  {
    if (e.errors.size() == 2)
    {
      g_set_error(error, 0, 0, 
          "%s : %s : %s\n%s : %s : %s",
          e.errors[0].reason.c_str(),
          e.errors[0].desc.c_str(),
          e.errors[0].origin.c_str(),
          e.errors[1].reason.c_str(),
          e.errors[1].desc.c_str(),
          e.errors[1].origin.c_str());
      return;
    }
    if (e.errors.size() == 3)
    {
      g_set_error(error, 0, 0, 
          "%s : %s : %s\n%s : %s : %s\n%s : %s : %s",
          e.errors[0].reason.c_str(),
          e.errors[0].desc.c_str(),
          e.errors[0].origin.c_str(),
          e.errors[1].reason.c_str(),
          e.errors[1].desc.c_str(),
          e.errors[1].origin.c_str(),
          e.errors[2].reason.c_str(),
          e.errors[2].desc.c_str(),
          e.errors[2].origin.c_str());
      return;
    }

    g_set_error(error, 0, 0, 
        "%s : %s : %s",
        e.errors[0].reason.c_str(),
        e.errors[0].desc.c_str(),
        e.errors[0].origin.c_str());
  }

}


void pylon_camera_new(const char* lib_path, const char* camera_ip, GError** error)
{

  g_assert(!pGrabber);
  try {

    //pImageMutex = new yat::Mutex;
    //pImageCondition = new yat::Condition(*pImageMutex);

    yat::PlugInManager pm;
    std::pair<yat::IPlugInInfo*, yat::IPlugInFactory*> pp = 
      pm.load((std::string(lib_path) + "/libbaslerpylon.so").c_str());

    yat::IPlugInObject* plugin_obj = 0;
    pp.second->create(plugin_obj);
    pGrabber = static_cast<GrabAPI::IGrabber*>(plugin_obj);

    yat::PlugInPropValues values;
    values["CameraIP"] = yat::Any(std::string(camera_ip));
    pGrabber->set_properties(values);
    pGrabber->initialize();
    pGrabber->set_image_handler(GrabAPI::ImageHandlerCallback::instanciate(handle_image));
    pGrabber->open();

  } 
  catch (const yat::Exception& e) {
    yat_exception_to_gerror(e, error);
  }
}

void pylon_camera_set_exposure_time(gdouble exp_time, GError** error)
{
  g_assert(pGrabber);
  try
  {
    pGrabber->set_exposure_time(yat::Any(exp_time));
  }
  catch (const yat::Exception& e)
  {
    yat_exception_to_gerror(e, error);
  }
}

void pylon_camera_get_exposure_time(gdouble* exp_time, GError** error)
{
  g_assert(pGrabber);
  try
  {
    yat::Any exp_time_result;
    pGrabber->get_exposure_time(exp_time_result);
    *exp_time = yat::any_cast<gdouble>(exp_time_result);
  }
  catch (const yat::Exception& e)
  {
    yat_exception_to_gerror(e, error);
  }
}


void pylon_camera_get_sensor_size(guint* width, guint* height, GError** error)
{
  g_assert(pGrabber);
  try
  {
    yat::Any w, h;
    pGrabber->get_sensor_width(w);
    pGrabber->get_sensor_height(h);
    *width = yat::any_cast<yat_int32_t>(w);
    *height = yat::any_cast<yat_int32_t>(h);
  }
  catch (const yat::Exception& e)
  {
    yat_exception_to_gerror(e, error);
  }
}

void pylon_camera_get_bit_depth(guint* depth, GError** error)
{
  g_assert(pGrabber);
  try
  {
    yat::Any bit_depth_result;
    pGrabber->get_bit_depth(bit_depth_result);
    *depth = yat::any_cast<yat_int32_t>(bit_depth_result);
  }
  catch (const yat::Exception& e)
  {
    yat_exception_to_gerror(e, error);
  }
}

void pylon_camera_start_acquision(GError** error)
{
  g_assert(pGrabber);
  //g_assert(pImageMutex);
  try
  {
    {
      //yat::MutexLock lock(pImageMutex);
      imageCounter = 0;
      currentImage = 0;
    }
    pGrabber->start();
  }
  catch (const yat::Exception& e)
  {
    yat_exception_to_gerror(e, error);
  }
}

void pylon_camera_stop_acquision(GError** error)
{
  g_assert(pGrabber);
  try
  {
    pGrabber->stop();
  }
  catch (const yat::Exception& e)
  {
    yat_exception_to_gerror(e, error);
  }
}

void pylon_camera_grab(gpointer *data, GError** error)
{
  g_assert(pGrabber);
  //g_assert(pImageMutex);
  //g_assert(pImageCondition);
  sleep(1);
  try
  {
      //yat::MutexLock lock(pImageMutex);
      g_assert(currentImage <= imageCounter);

      while (currentImage == imageCounter)
      {
        std::cerr << "wait for next image... " << currentImage << std::endl;
        //pImageCondition.wait();
      }

      std::cerr << "grab next image " << currentImage << ", " << imageCounter << std::endl;
      g_assert(currentImage < imageCounter);
      currentImage = imageCounter;
      //memcpy(*data, image->base(), image->width() * image->height() * 2);

  }
  catch (const yat::Exception& e)
  {
    yat_exception_to_gerror(e, error);
  }
}

void pylon_camera_delete()
{
  if (!pGrabber)
    return;

  pGrabber->close();
  pGrabber->uninitialize();
  delete pGrabber;
}

