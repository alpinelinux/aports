-- Create tables needed for PL/Tcl autoloading. This script should be run by
-- the database administrator only.
--
-- Statements in this script are extracted from pltcl_loadmod script.
--
-- Author: G.J.R. Timmer
-- Date: 2017-01-28

create table pltcl_modules (modname name, modseq int2, modsrc text);
create index pltcl_modules_i on pltcl_modules using btree (modname name_ops);

create table pltcl_modfuncs (funcname name, modname name);
create index pltcl_modfuncs_i on pltcl_modfuncs using hash (funcname name_ops);
