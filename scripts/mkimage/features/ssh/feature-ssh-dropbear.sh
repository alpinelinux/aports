# ssh_provider: dropbear
feature_ssh_dropbear() {
	ssh_provider="dropbear"
	add_rootfs_apks "dropbear dbclient"
}

feature_ssh_autostart_dropbear() {
	ssh_provider="dropbear"
	feature_ssh_dropbear
	add_overlays "ssh_autostart"
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

feature_ssh_generate_keys_dropbear() {
	ssh_provider="dropbear"
	add_host_apks "dropbear dropbear-convert"
	feature_ssh_dropbear
	add_overlays "ssh_generate_keys"
}


ovl_script_ssh_generate_keys_dropbear() {

	# Host keys destination:
	local h_keys="/etc/dropbear"
	ovl_mkdir -p "$h_keys"
	ovl_chmod 0700 "$h_keys"

	# Root keys destination:
	local r_ssh="/root/.ssh"
	local r_keys="$r_ssh/$ssh_provider"
	local a_keys="$r_ssh/authorized_keys"
	ovl_mkdir -p "$r_ssh"
	ovl_mkdir -p "$r_keys"
	ovl_chmod 0700 "$r_ssh"
	ovl_chmod 0700 "$r_keys"


	# Output directory for local copy of keys:
	keys_dir="${DESTDIR}/keys/${ovl_hostname}/$ssh_provider"
	mkdir -p "$keys_dir"
	chmod 0700 "$keys_dir"

	# Generate keys for each known type.
	local kt
	for kt in rsa dss ecdsa ; do
		local k_host="dropbear_${kt}_host_key"
		local k_id="id_${kt}_dropbear"
		local k_root_login="${k_id}.${ovl_hostname}.root_login"
		local k_user_root="${k_id}.${ovl_hostname}.user_root"

		# Host keys:
		dropbearkey -t $kt -f "${keys_dir}/${k_host}"
		dropbearkey -y -f "${keys_dir}/${k_host}" | grep "^ssh-${kt}" >  "${keys_dir}/${k_host}.pub"

		ovl_fkrt_enable
		(	cp "${keys_dir}/${k_host}" "$(ovl_get_root)/${h_keys#/}"
			cp "${keys_dir}/${k_host}.pub" "$(ovl_get_root)/${h_keys#/}"
		)
		ovl_fkrt_disable

		ovl_chmod 0600 "${h_keys}/${k_host}"

		rm "${keys_dir}/${k_host}"


		# Root user keys:
		dropbearkey -t $kt -f "${keys_dir}/${k_user_root}"
		dropbearkey -y -f "${keys_dir}/${k_user_root}" | grep "^ssh-${kt}" >  "${keys_dir}/${k_user_root}.pub"
		dropbearconvert dropbear "${keys_dir}/${k_user_root}" openssh "${keys_dir}/${k_user_root/_dropbear/}" 

		ovl_fkrt_enable
		(	cp "${keys_dir}/${k_user_root/_dropbear/}" "$(ovl_get_root)/${r_keys#/}"
			cp "${keys_dir}/${k_user_root}" "$(ovl_get_root)/${r_keys#/}"
			cp "${keys_dir}/${k_user_root}.pub" "$(ovl_get_root)/${r_keys#/}"
			[ "kt" = "ecdsa" ] && cp "${keys_dir}/${k_user_root}" "$(ovl_get_root)/${r_ssh#/}/id_dropbear"
		)
		ovl_fkrt_disable

		ovl_chmod 0600 "${r_keys}/${k_user_root}"
		ovl_chmod 0600 "${r_keys}/${k_user_root/_dropbear/}"

		rm "${keys_dir}/${k_user_root}"
		rm "${keys_dir}/${k_user_root/_dropbear/}"


		# Root login keys:
		ssh-keygen -q -o -N '' -C "SSH login key for root@$ovl_hostname" -t $kt -f "${keys_dir}/${k_root_login}"
		ssh-keygen -y -f "${keys_dir}/${k_root_login}" | grep "^ssh-${kt}" > "${keys_dir}/${k_root_login}.pub"
		dropbearconvert dropbear "${keys_dir}/${k_root_login}" openssh "${keys_dir}/${k_root_login/_dropbear/}" 

		ovl_fkrt_enable
		(	cp "${keys_dir}/${k_root_login}.pub" "$(ovl_get_root)/${r_keys#/}"
			cat "${keys_dir}/${k_root_login}.pub" >> "$(ovl_get_root)/${a_keys#/}"
		)
		ovl_fkrt_disable

	done

	# Lock down permissions on authorized_keys and id_dropbear private key.
	ovl_chmod 0600 "$a_keys"
	ovl_chmod 0600 "$r_ssh/id_dropbear"
}


