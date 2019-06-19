# Template to generate .drone.yml:
#   drone jsonnet --stream .

local pipeline(name, arch, command) = {
  kind: 'pipeline',
  name: name,
  clone: {
    disable: true,
  },
  platform: {
    os: 'linux',
    arch: arch,
  },
  steps: [
    {
      name: 'build',
      image: 'alpinelinux/alpine-drone-ci:' + name,
      pull: 'always',
      commands: [ command ],
      environment: {
        GH_TOKEN: {
          from_secret: 'github_token',
        },
      },
    },
  ],
  trigger: {
    event: [ 'pull_request' ],
  },
};

[
  pipeline('edge-x86', 'amd64', 'linux32 build.sh'),
  pipeline('edge-x86_64', 'amd64', 'build.sh'),
  pipeline('edge-aarch64', 'arm64', 'build.sh'),
  pipeline('edge-armhf', 'arm', 'build.sh'),
  pipeline('edge-armv7', 'arm', 'build.sh'),
]
