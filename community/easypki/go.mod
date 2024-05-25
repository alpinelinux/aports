module easypki

go 1.16

require (
	github.com/boltdb/bolt v1.2.2-0.20160719165138-5cc10bbbc5c1
	github.com/codegangsta/cli v1.19.2-0.20170506042529-d70f47eeca3a
	github.com/go-yaml/yaml v0.0.0-20170407172122-cd8b52f8269e
	github.com/google/easypki v1.1.1-0.20170217101540-d7ae2721b826
	gopkg.in/check.v1 v1.0.0-20201130134442-10cb98267c6c // indirect
	gopkg.in/yaml.v2 v2.4.0 // indirect
)

replace github.com/boltdb/bolt => gitlab.alpinelinux.org/kdaudt/bolt v1.3.5
