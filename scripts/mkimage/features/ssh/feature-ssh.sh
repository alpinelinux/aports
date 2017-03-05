feature_ssh() {
	local _arg _opt _val

	while [ "$1" ] ; do
		_arg="$1"
		shift
		_opt="${_arg%=*}"
		_val="${_arg#*=}"
		case "$_opt" in
			provider )
				case "$_val" in
					openssh | dropbear ) ssh_provider="$_val" ;;
					* ) warning "unrecognized ssh provider '$_val'" ;;
				esac ;;
			autostart )
				add_overlays "ssh_autostart" ;;
			generate_keys )
				add_overlays "ssh_generate_keys" ;;
		esac
	done

	ssh_provider="${ssh_provider:-openssh}"
	[ "$ssh_provider" ] && feature_ssh_$ssh_provider
}

overlay_ssh_autostart() {
	_call="${ssh_provider:+ovl_script_ssh_autostart_keys_${ssh_provider}}"
}

overlay_ssh_generate_keys() {
	_call="${ssh_provider:+ovl_script_ssh_generate_keys_${ssh_provider}}"
}


# ssh_provider: openssh 
feature_ssh_openssh() {
	add_apks_host "openssh"
	add_apks "openssh"
}

ovl_script_ssh_autostart_openssh() {
	ovl_runlevel_add boot sshd
}


ovl_script_ssh_generate_keys_openssh() {
	# Host keys destination
	ovl_mkdir /etc/ssh
	ovl_chmod 0700 /etc/ssh

	# Root keys destination
	ovl_mkdir /root/.ssh
	ovl_chmod 0700 /root/.ssh

	# Output directory for copy of keys.
	keys_dir="${DESTDIR}/keys/${ovl_hostname}"
	mkdir -p "$keys_dir"
	chmod 0700 "$keys_dir"

	# Generate keys for each known type
	local kt
	for kt in dsa ecdsa ed25519 rsa ; do

		local k_etc="$(ovl_get_root)/etc/dropbear/ssh_host_${kt}_key"
		local k_root="$(ovl_get_root)/root/.ssh/id_${kt}"

		ovl_fkrt_enable
		(	# Host keys
			ssh-keygen -q -o -N '' -C "$ovl_hostname" -t $kt -f "$k_etc"
			ssh-keygen -y -f "$k_etc" | grep "^ssh-${kt}" > "${k_etc}.pub"

			# Root keys
			ssh-keygen -q -o -N '' -C "root@$ovl_hostname" -t $kt -f "${k_root}_dropbear"
			ssh-keygen -y -f "${k_root}_dropbear" | grep "^ssh-${kt}" > "${k_root}.pub"
			cat "${k_root}.pub" >> "$(ovl_get_root)/authorized_keys"

		)
		ovl_fkrt_disable

		# Lock down permissions on keys
		ovl_chmod 0600 "${k_etc}"
		ovl_chmod 0600 "${k_root}.pub"

		# Copy host's public key and all root's keys to output directory.
		cp "${k_etc}.pub" "$keys_dir"
		cp "$k_root" "${k_root}.pub" "$keys_dir"

		# Remove root's private keys from overlay
		ovl_rm -f "/root/.ssh/${k_root##*/}"
	done

	# Lock down permissions on authorized_keys
	ovl_chmod 0600 "/root/.ssh/authorized_keys"

}


# ssh_provider: dropbear
feature_ssh_dropbear() {
	add_apks_host "dropbear dropbear-convert"
	add_apks "dropbear"
}

ovl_script_ssh_autostart_dropbear() {

	ovl_runlevel_add boot dropbear
	ovl_mkdir /etc/dropbear
	ovl_chmod 0700 /etc/dropbear

	ovl_create_file root:root 0644 /etc/conf.d/dropbear <<-EOF
		# Dropbear SSH server configuration:
		# -R Automatically create keys if they don't exist on first connection
		# -g Disable password login for root.
		DROPBEAR_OPTS="-R -g"
	EOF
}

ovl_script_ssh_generate_keys_dropbear() {
	# Host keys destination
	ovl_mkdir /etc/dropbear
	ovl_chmod 0700 /etc/dropbear

	# Root keys destination
	ovl_mkdir /root/.ssh
	ovl_chmod 0700 /root/.ssh

	# Output directory for copy of keys.
	keys_dir="${DESTDIR}/keys/${ovl_hostname}"
	mkdir -p "$keys_dir"
	chmod 0700 "$keys_dir"

	# Generate keys for each known type
	local kt
	for kt in rsa dss ecdsa ; do

		local k_etc="$(ovl_get_root)/etc/dropbear/dropbear_${kt}_host_key"
		local k_root="$(ovl_get_root)/root/.ssh/id_${kt}"

		ovl_fkrt_enable
		(	# Host keys
			dropbearkey -t $kt -f "$k_etc"
			dropbearkey -y -f "$k_etc" | grep "^ssh-${kt}" > "${k_etc}.pub"

			# Root keys
			dropbearkey -t $kt -f "${k_root}_dropbear"
			dropbearconvert dropbear "${k_root}_dropbear" openssh "${k_root}" 
			dropbearkey -y -f "${k_root}_dropbear" | grep "^ssh-${kt}" > "${k_root}.pub"
			cat "${k_root}.pub" >> "$(ovl_get_root)/authorized_keys"

		)
		ovl_fkrt_disable

		# Lock down permissions on keys
		ovl_chmod 0600 "${k_etc}"
		ovl_chmod 0600 "${k_root}.pub"

		# Copy host's public key and all root's keys to output directory.
		cp "${k_etc}.pub" "$keys_dir"
		cp "$k_root" "${k_root}_dropbear" "${k_root}.pub" "$keys_dir"

		# Remove root's private keys from overlay
		ovl_rm -f "/root/.ssh/${k_root##*/}" "/root/.ssh/${k_root##*/}_dropbear"
	done

	# Lock down permissions on authorized_keys
	ovl_chmod 0600 "/root/.ssh/authorized_keys"

}

