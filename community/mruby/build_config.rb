MRuby::Build.new do |conf|
  toolchain :gcc

  # include the default GEMs
  conf.gembox 'default'
end

MRuby::Build.new('host-debug') do |conf|
  toolchain :gcc

  # include the default GEMs
  conf.gembox 'default'

  # C compiler settings
  conf.cc.defines = %w(MRB_ENABLE_DEBUG_HOOK)

  # Generate mruby debugger command (require mruby-eval)
  conf.gem :core => "mruby-bin-debugger"
end
