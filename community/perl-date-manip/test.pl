use Date::Manip;

$dmt = new Date::Manip::TZ;
$obj = $dmt->base();
$dmt->config("forcedate","now,America/New_York");

print $obj->days_since_1BC([1,1,1]),"\n";
print $obj->days_since_1BC([2,1,1]),"\n";

