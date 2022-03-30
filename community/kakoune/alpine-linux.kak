# package build description file
hook global BufCreate (.*/)?APKBUILD %{
    set-option buffer filetype sh
}
