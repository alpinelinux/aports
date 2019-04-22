// keep N-day worth of data
var days=14;

// change to false to have the script to really exclude old records
// from the database. While true, no change at all will be made to the DB
var dryrun=true;

var now = new Date().getTime(),
	time_criteria = now - days * 86400 * 1000,
	time_criteria_in_seconds = time_criteria / 1000;

print((dryrun ? "[dryrun] " : "") + "pruning data older than " + days + " days (" + time_criteria + ")... ");

use ace;
var collectionNames = db.getCollectionNames();
for (i=0; i<collectionNames.length; i++) {
	var name = collectionNames[i];
	var query = null;

	if (name === 'event' || name === 'alarm') {
		query = {time: {$lt:time_criteria}};
	}

	// rogue ap
	if (name === 'rogue') {
		query = {last_seen: {$lt:time_criteria_in_seconds}};
	}

	// removes vouchers expired more than '$days' ago
	// active and unused vouchers are NOT touched
	if (name === 'voucher') {
		query = {end_time: {$lt:time_criteria_in_seconds}};
	}

	// guest authorization
	if (name === 'guest') {
		query = {end: {$lt:time_criteria_in_seconds}};
	}

	// if an user was only seen ONCE, $last_seen will not be defined
	// so, if $last_seen not defined, lets use $first_seen instead
	// also check if $blocked or $use_fixedip is set. If true, do NOT purge the
	// entry no matter how old it is. We want blocked/fixed_ip users to continue
	// blocked/fixed_ip
	if (name === 'user') {
		query = { blocked: { $ne: true}, use_fixedip: { $ne: true}, $or: [
				{last_seen: {$lt:time_criteria_in_seconds} },
				{last_seen: {$exists: false}, first_seen: {$lt:time_criteria_in_seconds} }
			]
		};
	}

	if (query) {
		count1 = db.getCollection(name).count();
		count2 = db.getCollection(name).find(query).count();
		print((dryrun ? "[dryrun] " : "") + "pruning " + count2 + " entries (total " + count1 + ") from " + name + "... ");
		if (!dryrun) {
			db.getCollection(name).remove(query);
			db.runCommand({ compact: name });
		}
	}
}

use ace_stat;
var collectionNames = db.getCollectionNames();
for (i=0; i<collectionNames.length; i++) {
	var name = collectionNames[i];
	var query = null;

	// historical stats (stat.*)
	if (name.indexOf('stat')==0) {
		query = {time: {$lt:time_criteria}};
	}

	if (query) {
		count1 = db.getCollection(name).count();
		count2 = db.getCollection(name).find(query).count();
		print((dryrun ? "[dryrun] " : "") + "pruning " + count2 + " entries (total " + count1 + ") from " + name + "... ");
		if (!dryrun) {
			db.getCollection(name).remove(query);
			db.runCommand({ compact: name });
		}
	}
}

if (!dryrun) db.repairDatabase();

