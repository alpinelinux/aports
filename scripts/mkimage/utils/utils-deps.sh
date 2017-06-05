# Usage: (index with indexed deps) | _manifest_deps_resolve_recurse [<additional deps files>...]
dep_solver_recurse_each() {
	awk -e '
		NF>0	{ ideps[$1]=$0 }
		function alldeps(ad,d, sd,   td, t) {
			split(ad[d], td)
			for (t in td) { if (td[t] in sd) { continue } ; sd[td[t]]=td[t] ; if (td[t] !=d) { alldeps(ad, td[t], sd) } }
		}
		END {
			for( i in ideps) {
				split("",all)
				alldeps(ideps, i, all)
				printf i ; for( j in all ) { if(j != "" && all[j] != "" && i!=j) { if (ideps[j] != "" ) { printf("\t%s", j) } else { printf("\t(%s)",j) } } } ; printf "\n"
			}
		}
	' "$@"
}

# Input: targetkey\tdepkey\tdepkey\t...
# Output: targetkey\tdepkey\tsubdepkey...\tdepkey\tsubdepkey...\t...
dep_solver_grow_tree() {
	while [ $# -gt 0  ] ; do
		case "$1" in
			fwd-deps ) local fwddeps=1 ;;
			broken-deps) local fwdbroke=1 ;;
			fwd-raw ) local fwdraw=1 ;;
			rev-raw ) local revraw=1 ;;
		esac
		shift
	done

	awk -v print_fwd_resolved=${fwddeps:-0} -v print_fwd_broken=${fwdbroke:-0} -v print_fwd_raw=${fwdraw:-0} -v print_rev_raw=${revraw:-0} -e '
	

	# Root node
	NF == 1 {
		tgt=$1
		roots[tgt]=tgt
		rawdeps[tgt]=""
	}


	# Non-root node
	NF > 1 {
		tgt=$1
		# Set initial number of deps left to resolve to total number of deps for target
		undefdeps[tgt]=NF-1
		for(x=2; x<=NF; x++) {
			# Track raw deps for this target for later reference
			if( rawdeps[tgt] != "") { rawdeps[tgt]=rawdeps[tgt] "\t" } 
			rawdeps[tgt]=rawdeps[tgt] $x
			
			# Track reverse deps for this target to build tree.
			if( revdeps[$x] != "") { revdeps[$x]=revdeps[$x] "\t" } 
			revdeps[$x]=revdeps[$x] tgt
		}
	}

	
	function grow_tree(undeps, rdeps, deps, base, branch,   rds, i) {
		# Add the base with deps to this nodes dep list and dec the number of unresolved deps.
		if (deps[branch] != "") { deps[branch]=deps[branch] "\t" } 
		deps[branch]=deps[branch] base
		if (deps[base] != "") { deps[branch]=deps[branch] "\t" deps[base] }
		
		# Dec undeps and delete entry from array once count reaches zero for target.
		undeps[branch]--
		if (undeps[branch] == 0) { delete undeps[branch] }

		# Iterate over the list of reverse deps for this node
		split(rdeps[branch], rds, "\t")
		for (i in rds) {
			# If a dep chain has already traversed the target node, add a link for this dep and continue
			if (rds[i] in deps) {
				deps[rds[i]]=deps[rds[i]] "\t" branch "\t" deps[branch]

				# Dec undeps and delete entry from array once count reaches zero for target.
				undeps[rds[i]]--
				if (undeps[rds[i]] == 0) { delete undeps[rds[i]] }
				continue
			}

			# Otherwise, grow the tree fron this node up.
			grow_tree(undeps,rdeps,deps,branch,rds[i])
		}
	}
	
	function broken_tree_grow_roots(rawds,broken_branch, sd,   td, t) {
		split(rawds[broken_branch], tempds)
		for (t in tempds) { if (tempds[t] in sd) { continue } ; sd[tempds[t]]=tempds[t] ; if (tempds[t] !=broken_branch) { broken_tree_grow_roots(rawds, tenpds[t], sd) } }
	}

	END {

		if (print_fwd_raw == 1 ) {
			print "# Immediate Forward Deps"
			for(f in rawdeps) {
				print f "\t" rawdeps[f]
			}
		}

		if (print_rev_raw == 1 ) {
			print "# Immediate Reverse Deps"
			for(r in revdeps) {
				print r "\t" revdeps[r]
			}
		}

		# Iterate over the list of root nodes and grow the tree upward from each.
		for(root in roots) {
			forwarddeps[root]=""
			revlist=revdeps[root]
			split(revlist, revs, "\t")
			for(v in revs) {
				grow_tree(undefdeps,revdeps,forwarddeps,root,revs[v])
			}
		}


		for (tgt in undefdeps) {
			split("",mydeps)
			broken_tree_grow_roots(rawdeps, tgt, mydeps)
			forwarddeps[tgt]=""
			for( j in mydeps ) {
				if(j != "" && mydeps[j] != "" && tgt!=j) {
					if(forwarddeps[tgt]!="") { forwarddeps[tgt]=forwarddeps[tgt] "\t" }
					if (rawdeps[j] != "" ) {
						forwarddeps[tgt]=fordwardeps[tgt] j
					} else {
						forwarddeps[tgt]=fordwardeps[tgt] "(" j ")"
						missingroots[j]=j
					} 
				}
			}
		}
		
		if (print_fwd_resolved == 1) {
			print "# Forward Dep Chains"
			for(f in forwarddeps) {
				#if(f in undefdeps) { continue }
				print f "\t" forwarddeps[f]
			}
		}

		if (print_fwd_broken == 1) {
			# Iterate over remaining keys of undefdeps.
			# Backreference to find root broken deps and build unresolved dep tree.
			for(tgt in undefdeps) {
				rawlist=rawdeps[tgt]
				split(rawlist, raws, "\t")
				for(r in raws) {
					# Skip nodes not at base of broken tree.
					if(raws[r] in rawdeps) { continue }
					# Mark missing deps at root of broken dep chain with parens. 
					if (brokendeps[tgt] != "") { brokendeps[tgt]=brokendeps[tgt] "\t" } 
					brokendeps[tgt]=brokendeps[tgt] "(" raws[r] ")"
					revlist=revdeps[tgt]
					split(revlist, revs, "\t")
					for(v in revs) {
						if (revs[v] in brokendeps) { continue }
						grow_tree(undefdeps,revdeps,brokendeps,tgt,revs[v])
					}
				}
			}

			print "# Broken Dep Chains"
			for(b in brokendeps) {
				print b "\t" brokendeps[b]
			}
		}

		if (print_fwd_broken == 2) {
			# Iterate over remaining keys of undefdeps.
			# Backreference to find root broken deps and build unresolved dep tree.
			for(tgt in undefdeps) {
				rawlist=rawdeps[tgt]
				split(rawlist, raws, "\t")
				for(r in raws) {
					# Skip nodes not at base of broken tree.
					if(raws[r] in rawdeps) { continue }
					# Mark missing deps at root of broken dep chain with parens. 
					if (brokendeps[tgt] != "") { brokendeps[tgt]=brokendeps[tgt] "\t" } 
					brokendeps[tgt]=brokendeps[tgt] "(" raws[r] ")"
					revlist=revdeps[tgt]
					split(revlist, revs, "\t")
					for(v in revs) {
						if (revs[v] in brokendeps) { continue }
						grow_tree(undefdeps,revdeps,brokendeps,tgt,revs[v])
					}
				}
			}

			print "# Broken Dep Chains"
			for(b in brokendeps) {
				print b "\t" brokendeps[b]
			}
		}

	}
	'
}


# Input: targetkey\tdepkey\tdepkey\t...
# Output: targetkey\tdepkey\tsubdepkey...\tdepkey\tsubdepkey...\t...
dep_solver_grow_tree() {
	while [ $# -gt 0  ] ; do
		case "$1" in
			fwd-deps ) local fwddeps=1 ;;
			broken-deps) local fwdbroke=1 ;;
			fwd-raw ) local fwdraw=1 ;;
			rev-raw ) local revraw=1 ;;
		esac
		shift
	done

	awk -v print_fwd_resolved=${fwddeps:-0} -v print_fwd_broken=${fwdbroke:-0} -v print_fwd_raw=${fwdraw:-0} -v print_rev_raw=${revraw:-0} -e '
	

	# Root node
	NF == 1 {
		tgt=$1
		roots[tgt]=tgt
		rawdeps[tgt]=""
	}


	# Non-root node
	NF > 1 {
		tgt=$1
		# Set initial number of deps left to resolve to total number of deps for target
		undefdeps[tgt]=NF-1
		for(x=2; x<=NF; x++) {
			# Track raw deps for this target for later reference
			if( rawdeps[tgt] != "") { rawdeps[tgt]=rawdeps[tgt] "\t" } 
			rawdeps[tgt]=rawdeps[tgt] $x
			
			# Track reverse deps for this target to build tree.
			if( revdeps[$x] != "") { revdeps[$x]=revdeps[$x] "\t" } 
			revdeps[$x]=revdeps[$x] tgt
		}
	}

	
	function grow_tree(undeps, rdeps, deps, base, branch,   rds, i) {
		# Add the base with deps to this nodes dep list and dec the number of unresolved deps.
		if (deps[branch] != "") { deps[branch]=deps[branch] "\t" } 
		deps[branch]=deps[branch] base
		if (deps[base] != "") { deps[branch]=deps[branch] "\t" deps[base] }
		
		# Dec undeps and delete entry from array once count reaches zero for target.
		undeps[branch]--
		if (undeps[branch] == 0) { delete undeps[branch] }

		# Iterate over the list of reverse deps for this node
		split(rdeps[branch], rds, "\t")
		for (i in rds) {
			# If a dep chain has already traversed the target node, add a link for this dep and continue
			if (rds[i] in deps) {
				deps[rds[i]]=deps[rds[i]] "\t" branch "\t" deps[branch]

				# Dec undeps and delete entry from array once count reaches zero for target.
				undeps[rds[i]]--
				if (undeps[rds[i]] == 0) { delete undeps[rds[i]] }
				continue
			}

			# Otherwise, grow the tree fron this node up.
			grow_tree(undeps,rdeps,deps,branch,rds[i])
		}
	}

	END {

		if (print_fwd_raw == 1 ) {
			print "# Immediate Forward Deps"
			for(f in rawdeps) {
				print f "\t" rawdeps[f]
			}
		}

		if (print_rev_raw == 1 ) {
			print "# Immediate Reverse Deps"
			for(r in revdeps) {
				print r "\t" revdeps[r]
			}
		}

		# Iterate over the list of root nodes and grow the tree upward from each.
		for(root in roots) {
			forwarddeps[root]=""
			revlist=revdeps[root]
			split(revlist, revs, "\t")
			for(v in revs) {
				grow_tree(undefdeps,revdeps,forwarddeps,root,revs[v])
			}
		}
		
		if (print_fwd_resolved == 1) {
			print "# Forward Dep Chains"
			for(f in forwarddeps) {
				if(f in undefdeps) { continue }
				print f "\t" forwarddeps[f]
			}
		}

		if (print_fwd_broken == 1) {
			# Iterate over remaining keys of undefdeps.
			# Backreference to find root broken deps and build unresolved dep tree.
			for(tgt in undefdeps) {
				rawlist=rawdeps[tgt]
				split(rawlist, raws, "\t")
				for(r in raws) {
					# Skip nodes not at base of broken tree.
					if(raws[r] in rawdeps) { continue }
					# Mark missing deps at root of broken dep chain with parens. 
					if (brokendeps[tgt] != "") { brokendeps[tgt]=brokendeps[tgt] "\t" } 
					brokendeps[tgt]=brokendeps[tgt] "(" raws[r] ")"
					revlist=revdeps[tgt]
					split(revlist, revs, "\t")
					for(v in revs) {
						if (revs[v] in brokendeps) { continue }
						grow_tree(undefdeps,revdeps,brokendeps,tgt,revs[v])
					}
				}
			}

			print "# Broken Dep Chains"
			for(b in brokendeps) {
				print b "\t" brokendeps[b]
			}
		}

	}
	'
}

_test_dep_solve_tree() {
	_test_simple_dep_solver dep_solver_grow_tree fwd-deps broken-deps
	_test_simple_dep_solver dep_solver_recurse_each
}

_test_simple_dep_solver() {
	printf "\n## The following should succeed for all targets:\n"
	printf "\n# Minimal case, one root, no deps.\n"
	"$@" <<-EOF
	a
	EOF

	printf "\n# Minimal case, two roots, no deps.\n"
	"$@" <<-EOF
	a
	b
	EOF

	printf "\n# Minimal case, one root, one tgt with one dep.\n"
	"$@" <<-EOF
	a
	b	a
	EOF

	printf "\n# Simple case, one root, two targets with the same dep.\n"
	"$@" <<-EOF
	a
	b	a
	c	a
	EOF

	printf "\n# Simple case, two roots, two targets with independent deps.\n"
	"$@" <<-EOF
	a
	b
	c	a
	d	b
	EOF

	printf "\n# Simple case, two roots, one target depends on both.\n"
	"$@" <<-EOF
	a
	b
	c	a	b
	EOF

	printf "\n# Minimal chain, one root with one target with direct depending on it, which in turn has one target depending on it.\n"
	"$@" <<-EOF
	a
	b	a
	c	b
	EOF

	printf "\n# Minimal branched tree, one root with one target with direct depending on it, which in turn has two targets depending on it.\n"
	"$@" <<-EOF
	a
	b	a
	c	b
	d	b
	EOF

	printf "\n## The following should show unresolved deps for all targets:\n"


	printf "\n# Minimal case, no root, one tgt with one unresolved root.\n"
	"$@" <<-EOF
	b	a
	EOF

	printf "\n# Simple case, no root, two tgts with same unresolved root.\n"
	"$@" <<-EOF
	b	a
	c	a
	EOF
	
	printf "\n# Minimal broken chain, no root, one tgt with one unresolved root, with one target depending upon it.\n"
	"$@" <<-EOF
	b	a
	c	b
	EOF

	printf "\n# Minimal broken branched tree, no root, one tgt with one unresolved root, with two targets depending upon it.\n"
	"$@" <<-EOF
	b	a
	c	b
	d	b
	EOF


	printf "\n# All succeed.\n"
	"$@" <<-EOF
	d	c	b	a
	c	a
	b
	a
	EOF

	printf "\n# 'a' missing, only 'b' succeeds.\n"
	"$@" <<-EOF
	d	c	b	a
	c	a
	b
	EOF
	
	printf "\n# Circular dep, none succeed.\n"
	"$@" <<-EOF
	c	b
	b	a
	a	c
	EOF
	
	printf "\n# Circular dep, all succeed.\n"
	"$@" <<-EOF
	c	b	a
	b	c
	a
	EOF


	printf "\n# Circular dep, all succeed.\n"
	"$@" <<-EOF
	d	c
	c	b	d	a
	b
	a
	EOF
}
