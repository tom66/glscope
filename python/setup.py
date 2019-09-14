from distutils.core import setup, Extension
setup(name = 'scope', version = '1.0',  \
   ext_modules = [Extension('scope',
                            sources=['scope.c', 'raspiraw.c', 'waveform.c', 'shader_utils.c'],
                            include_dirs=['/opt/vc/include'],
                            libraries=['m','glfw','epoxy','GLU', 'mmal', 'bcm_host', 'vcos', 'mmal_core',
                            'mmal_util', 'mmal_vc_client'],
                            library_dirs=['/opt/vc/lib'],
   )])
