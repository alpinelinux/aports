data_dir             = "/var/lib/nomad"
disable_update_check = true
# Logging is handled by supervise-daemon so disable
# Syslog to avoid double logging.
enable_syslog        = false
plugin_dir           = "/usr/lib/nomad/plugins"

server {
  enabled          = true
  bootstrap_expect = 1
}

client {
  enabled = true

  # CNI-related settings
  cni_config_dir = "/etc/cni"
  cni_path = "/usr/libexec/cni"

  options {
    # Uncomment to disable some drivers
    #driver.denylist = "java,raw_exec"

    # Disable some fingerprinting
    fingerprint.denylist = "env_aws,env_azure,env_digitalocean,env_gce"
  }
}

ui {
  # Uncomment to enable UI, it will listen on port 4646
  #enabled = true
}
