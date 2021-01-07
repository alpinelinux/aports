----
-- Configure Czech ispell dictionary for fulltext search.
--
-- If the dictionary and configuration already exists, it will do nothing.
----

\set cfgname 'czech'
\set dictname 'czech_ispell'
\set stemmer 'czech_stem'  -- change to 'simple' if you don't have czech stemmer
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
  CREATE TEXT SEARCH DICTIONARY :"dictname" (
    template = ispell,
    dictfile = czech,
    afffile = czech,
    stopwords = czech
  );
\endif

-- Update TS configuration

\echo updating text search configuration :"cfgname" in namespace :"namespace"
ALTER TEXT SEARCH CONFIGURATION :"cfgname"
  ALTER MAPPING FOR asciiword, asciihword, hword, hword_asciipart, hword_part, word WITH :"dictname", :"stemmer";
