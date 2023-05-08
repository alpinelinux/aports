#!/bin/sh

# JVM version supported by Neo4j.
JVM_VERSION=11

export NEO4J_HOME="${NEO4J_HOME:-"/var/lib/neo4j"}"
export NEO4J_CONF="${NEO4J_CONF:-"/etc/neo4j"}"
export JAVA_HOME="${JAVA_HOME:-${NEO4J_JAVA_HOME-}}"

if [ "${JAVA_HOME-}" ]; then
	if ! [ -x "$JAVA_HOME"/bin/java ]; then
		echo "$0: $JAVA_HOME/bin/java does not exist or is not executable!" >&2
		exit 2
	fi

	jvm_version=$("$JAVA_HOME"/bin/java -XshowSettings:properties -version 2>&1 \
		| sed -En 's/^\s*java\.specification\.version\s*=\s*(\d+).*/\1/p')

	if [ "$jvm_version" != "$JVM_VERSION" ]; then
		echo "$0: warning: Neo4j supports only JVM $JVM_VERSION, but you are running $jvm_version" >&2
	fi
else
	export JAVA_HOME="/usr/lib/jvm/java-$JVM_VERSION-openjdk"
fi

exec /usr/share/neo4j/bin/$(basename "$0") "$@"
