avformat_dep = dependency('libavformat')
avcodec_dep  = dependency('libavcodec')
avutil_dep   = dependency('libavutil')

shared_module('waveform-qt',
  'waveform_qt.cc',
  dependencies: [audacious_dep, qt_dep, avformat_dep, avcodec_dep, avutil_dep],
  name_prefix: '',
  install: true,
  install_dir: visualization_plugin_dir
)
