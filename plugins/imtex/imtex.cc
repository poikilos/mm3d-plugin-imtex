#include "imtex.h"

#include "config.h"
#include "log.h"
#include "texmgr.h"

#include <X11/Xlib.h>
#include <Imlib2.h>
#include <string>

// See below for plugin functions

using std::list;
using std::string;

//------------------------------------------------------------------
// ImLib2 Texture Filter implementation
//------------------------------------------------------------------

static ImlibTextureFilter * s_filter = NULL;

ImlibTextureFilter::ImlibTextureFilter()
{
   m_read.push_back( "GIF" );
   m_read.push_back( "PNM" );
   m_read.push_back( "TIF" );
   m_read.push_back( "TIFF" );
}

ImlibTextureFilter::~ImlibTextureFilter()
{
}

bool ImlibTextureFilter::canRead( const char * filename )
{
   if ( filename == NULL )
   {
      return false;
   }

   string cmpstr;
   unsigned len = strlen( filename );

   list<string>::iterator it;
   
   for ( it = m_read.begin(); it != m_read.end(); it++ )
   {
      cmpstr = string(".") + *it;

      if ( len >= cmpstr.length() )
      {
         if ( strcasecmp( &filename[len-cmpstr.length()], cmpstr.c_str()) == 0 )
         {
            return true;
         }
      }
   }

   return false;
}

Texture::Error ImlibTextureFilter::readFile( Texture * texture, const char * filename )
{
   Imlib_Image image;
   Imlib_Load_Error imError;

   if ( filename == NULL || texture == NULL )
   {
      log_error( "filename or texture is NULL\n" );
      return Texture::ERROR_BAD_ARGUMENT;
   }

   image = imlib_load_image_with_error_return( filename, &imError );

   if ( image )
   {
      imlib_context_set_image( image );

      bool hasAlpha = imlib_image_has_alpha() ? true : false;
      log_debug( "Alpha channel: %s\n", hasAlpha ? "present" : "not present" );

      texture->m_width  = imlib_image_get_width();
      texture->m_height = imlib_image_get_height();

      DATA32 * imageData = imlib_image_get_data_for_reading_only();
   
      unsigned pixelBytes = hasAlpha ? 4 : 3;
      unsigned pixelCount = texture->m_width * texture->m_height;
      unsigned imageSize = pixelCount * (pixelBytes * sizeof(uint8_t));
      texture->m_data = new uint8_t[ imageSize ];

      if ( hasAlpha )
      {
         texture->m_format = Texture::FORMAT_RGBA;

         // Convert ARGB (Imlib, imageData) to RGBA (OpenGL, texture->m_data)
         // And make bottom row the first row, as required by OpenGL
         for ( int y = 0; y < texture->m_height; y ++ )
         {
            for ( int x = 0; x < texture->m_width; x++ )
            {
               texture->m_data[ ((y * texture->m_width + x)*4) + 0 ] 
                  = ((imageData[((texture->m_height - y - 1) * texture->m_width) + x] 
                           & 0x00FF0000) >> 16);
               texture->m_data[ ((y * texture->m_width + x)*4) + 1 ] 
                  = ((imageData[((texture->m_height - y - 1) * texture->m_width) + x] 
                           & 0x0000FF00) >>  8);
               texture->m_data[ ((y * texture->m_width + x)*4) + 2 ] 
                  = ((imageData[((texture->m_height - y - 1) * texture->m_width) + x] 
                           & 0x000000FF) >>  0);
               texture->m_data[ ((y * texture->m_width + x)*4) + 3 ] 
                  = ((imageData[((texture->m_height - y - 1) * texture->m_width) + x] 
                           & 0xFF000000) >> 24);
            }
         }
      }
      else
      {
         texture->m_format = Texture::FORMAT_RGB;

         // Convert ARGB (Imlib, imageData) to RGB (OpenGL, texture->m_data)
         // And make bottom row the first row, as required by OpenGL
         for ( int y = 0; y < texture->m_height; y ++ )
         {
            for ( int x = 0; x < texture->m_width; x++ )
            {
               texture->m_data[ ((y * texture->m_width + x)*3) + 0 ] 
                  = ((imageData[((texture->m_height - y - 1) * texture->m_width) + x] 
                           & 0x00FF0000) >> 16);
               texture->m_data[ ((y * texture->m_width + x)*3) + 1 ] 
                  = ((imageData[((texture->m_height - y - 1) * texture->m_width) + x] 
                           & 0x0000FF00) >>  8);
               texture->m_data[ ((y * texture->m_width + x)*3) + 2 ] 
                  = ((imageData[((texture->m_height - y - 1) * texture->m_width) + x] 
                           & 0x000000FF) >>  0);
            }
         }
      }

      // We do our own caching... twice... thanks anyway
      imlib_free_image_and_decache(); 

      texture->m_filename = strdup( filename );

      const char * name = rindex( filename, DIR_SLASH );
      if ( name )
      {
         texture->m_name = strdup( &name[1] );
      }
      else
      {
         texture->m_name = strdup( filename );
      }
      char * ext = rindex( texture->m_name, '.' );
      if ( ext )
      {
         ext[0] = '\0';
      }

      return Texture::ERROR_NONE;
   }
   else
   {
      switch( imError )
      {
         case IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT:
         case IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY:
         case IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST:
            return Texture::ERROR_NO_FILE;
         case IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY:
            return Texture::ERROR_BAD_MAGIC;
         case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ:
            return Texture::ERROR_ACCESS_DENIED;
         case IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT:
            return Texture::ERROR_UNSUPPORTED_VERSION;
         default:
            log_error( "Imlib error: %d\n", imError );
            break;
      }
   }

   return Texture::ERROR_UNKNOWN;
}

list<string> ImlibTextureFilter::getReadTypes()
{
   list<string> rval;

   list<string>::iterator it;
   for ( it = m_read.begin(); it != m_read.end(); it++ )
   {
      rval.push_back( string("*.") + *it );
   }

   return rval;
}

list<string> ImlibTextureFilter::getWriteTypes()
{
   list<string> rval;

   list<string>::iterator it;
   for ( it = m_write.begin(); it != m_write.end(); it++ )
   {
      rval.push_back( string("*.") + *it );
   }

   return rval;
}

// This ifdef is present in case someone wants to compile the filter statically
// to Misfit Model 3D (or another program that uses the TextureFilterManager)
//
// If your plugin will always behave as a plugin, you don't need to worry about
// this.
//
#ifdef PLUGIN

//------------------------------------------------------------------
// Plugin functions
//------------------------------------------------------------------

// Create a texture filter object and register it with the texture manager
extern "C" bool plugin_init()
{
   if ( s_filter == NULL )
   {
      s_filter = new ImlibTextureFilter();
      TextureManager * texmgr = TextureManager::getInstance();
      texmgr->registerTextureFilter( s_filter );
   }
   log_debug( "ImLib2 texture filture plugin initialized\n" );
   return true;
}

// The texture manager will delete our registered filter.
// We have no other cleanup to do
extern "C" bool plugin_uninit()
{
   s_filter = NULL; // TextureManager deletes filters
   log_debug( "ImLib2 texture filture plugin uninitialized\n" );
   return true;
}

extern "C" const char * plugin_version()
{
   return "0.9.0";
}

extern "C" const char * plugin_desc()
{
   return "ImLib2 texture filture";
}

#endif // PLUGIN