
# ssh_provider: openssh 
feature_ssh_openssh() {
	ssh_provider="openssh"
	add_apks "openssh"
}


feature_ssh_autostart_openssh() {
	feature_ssh_openssh
	add_rootfs_apks "openssh"
	add_overlays "ssh_autostart"
}

ovl_script_ssh_autostart_openssh() {
	ovl_runlevel_add boot sshd
}


feature_ssh_generate_keys_openssh() {
	feature_ssh_openssh
	add_host_apks "openssh-keygen"
	add_overlays "ssh_generate_keys"
}


ovl_script_ssh_generate_keys_openssh() {

	# Host keys destination:
	local h_keys="/etc/ssh"
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
	for kt in dsa ecdsa ed25519 rsa ; do
		local k_host="ssh_host_${kt}_key"
		local k_id="id_${kt}"
		local k_root_login="${k_id}.${ovl_hostname}.root_login"
		local k_user_root="${k_id}.${ovl_hostname}.user_root"

		# Host keys:
		ssh-keygen -q -o -N '' -C "$ovl_hostname host key" -t $kt -f "${keys_dir}/${k_host}"
		ssh-keygen -y -f "${keys_dir}/${k_host}" > "${keys_dir}/${k_host}.pub"

		ovl_fkrt_enable
		(	cp "${keys_dir}/${k_host}" "$(ovl_get_root)/${h_keys#/}"
			cp "${keys_dir}/${k_host}.pub" "$(ovl_get_root)/${h_keys#/}"
		)
		ovl_fkrt_disable
		ovl_chmod 0600 "${h_keys}/${k_host}"
		rm "${keys_dir}/${k_host}"


		# Root user keys:
		ssh-keygen -q -o -N '' -C "root@$ovl_hostname" -t $kt -f "${keys_dir}/${k_user_root}"
		ssh-keygen -y -f "${keys_dir}/${k_user_root}" > "${keys_dir}/${k_user_root}.pub"

		ovl_fkrt_enable
		(	cp "${keys_dir}/${k_user_root}" "$(ovl_get_root)/${r_ssh#/}/${k_id}"
			cp "${keys_dir}/${k_user_root}" "$(ovl_get_root)/${r_keys#/}"
			cp "${keys_dir}/${k_user_root}.pub" "$(ovl_get_root)/${r_keys#/}"
		)
		ovl_fkrt_disable

		ovl_chmod 0600 "${r_keys}/${k_user_root}"
		ovl_chmod 0600 "${r_ssh}/${k_id}"

		rm "${keys_dir}/${k_user_root}"


		# Root login keys:
		ssh-keygen -q -o -N '' -C "SSH login key for root@$ovl_hostname" -t $kt -f "${keys_dir}/${k_root_login}"
		ssh-keygen -y -f "${keys_dir}/${k_root_login}" > "${keys_dir}/${k_root_login}.pub"

		ovl_fkrt_enable
		(	cp "${keys_dir}/${k_root_login}.pub" "$(ovl_get_root)/${r_keys#/}"
			cat "${keys_dir}/${k_root_login}.pub" >> "$(ovl_get_root)/${a_keys#/}"
		)
		ovl_fkrt_disable

	done

	# Lock down permissions on authorized_keys.
	ovl_chmod 0600 "$a_keys"

}
