MRuby::Build.new do |conf|
  conf.toolchain :gcc

  # include the default GEMs
  conf.gembox 'default'
end

MRuby::Build.new('host-debug') do |conf|
  conf.toolchain :gcc

  conf.enable_debug

  # include the default GEMs
  conf.gembox 'default'

  # C compiler settings
  conf.cc.defines = %w(MRB_USE_DEBUG_HOOK)

  # Generate mruby debugger command (require mruby-eval)
  conf.gem :core => "mruby-bin-debugger"
end
