----
-- Configure Czech ispell dictionary for fulltext search.
--
-- If the dictionary and configuration already exists, it will do nothing.
----

\set cfgname 'czech'
\set dictname 'czech_ispell'
\set namespace 'pg_catalog'

\out /dev/null
\timing off


SET search_path = :"namespace";

-- Create TS dictionary if not exists

SELECT EXISTS (SELECT 1 FROM pg_ts_dict WHERE dictnamespace = :'namespace'::regnamespace AND dictname = :'dictname') AS has_dict;
\gset

\if :has_dict
  \echo text search dictionary :"dictname" already exists in namespace :"namespace"
\else
  \echo creating text search dictionary :"dictname" in namespace :"namespace"
  CREATE TEXT SEARCH DICTIONARY :"dictname" (template=ispell, dictfile = czech, afffile=czech, stopwords=czech);
\endif

-- Create TS configuration if not exists

SELECT EXISTS (SELECT 1 FROM pg_ts_config WHERE cfgnamespace = :'namespace'::regnamespace AND cfgname = :'cfgname') AS has_config;
\gset

\if :has_config
  \echo text search configuration :"cfgname" already exists in namespace :"namespace"
\else
  \echo creating text search configuration :"cfgname" in namespace :"namespace"
  CREATE TEXT SEARCH CONFIGURATION :"cfgname" (copy=english);
  ALTER TEXT SEARCH CONFIGURATION :"cfgname" ALTER MAPPING FOR word, asciiword WITH :"dictname", simple;
\endif
