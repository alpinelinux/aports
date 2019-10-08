data_dir             = "/var/lib/nomad"
disable_update_check = true
enable_syslog        = true

server {
  enabled          = true
  bootstrap_expect = 1
}

client {
  enabled = true
}
