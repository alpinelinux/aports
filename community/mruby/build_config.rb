MRuby::Build.new do |conf|
  conf.toolchain :gcc

  # include the default GEMs
  conf.gembox 'default'

  conf.enable_bintest
  conf.enable_test

  # Configuration macros
  conf.cc.defines = %w(MRB_USE_READLINE MRB_UTF8_STRING)
end

MRuby::Build.new('host-debug') do |conf|
  conf.toolchain :gcc

  conf.enable_debug

  # include the default GEMs
  conf.gembox 'default'

  # Configuration macros
  conf.cc.defines = %w(MRB_USE_READLINE MRB_UTF8_STRING MRB_USE_DEBUG_HOOK)

  # Generate mruby debugger command (require mruby-eval)
  conf.gem :core => "mruby-bin-debugger"
end
