CENTRALA:.*_act:.*|CENTRALA_NOTIFY:.* { 
	### read device and reading names from CENTRALA attributes
	# valve actuators
	my $currentActReading = AttrVal("CENTRALA", "valveCurrentActReadingName", "pos_tx");;      # GET current  actuation reading name on the actor_device(s)
	my $setActReading 	  = AttrVal("CENTRALA", "valveSetActReadingName", "fht_pos_percent");; # SET required actuation reading name on the actor_device(s)
	# boiler
	my $boilerDevice 	  = AttrVal("CENTRALA", "boilerDevice", "MYSENSOR_KOTEL");;
	my $boilerSetReading  = AttrVal("CENTRALA", "boilerSetReadingName", "status1");;
	my $boilerActTreshold = AttrVal("CENTRALA", "boilerActTreshold", "0");; 
	# CENTRALA readings
	my $acts_str 		  = AttrVal("CENTRALA", "actList", "");; # space separated list <CENTRALA_req_act_reading>,<actor_device>,<weight>[space]...
	
	### parse data and sum all actuations (required and current)
	my @acts = split(' ', $acts_str);; # space separated
	my @T;;
	my $i=0;;
	my $sum_req = 0;;  # required actuations (weighted sum)
	my $sum_cur = 0;;  # current actuations (weighted sum)
	for my $a (@acts) {
	    # parse single actList atribute item (comma separated)
		my @s =  split(/,/, $a);; # comma separated
		my $req_name = $s[0];; # CENTRALA required actuation reading name
		my $act_dev  = $s[1];; # actor device name
		my $weight   = $s[2];; # actor weight
		# parse remaining readings
		my $req_val = ReadingsVal("CENTRALA", $req_name, 0);; # required actuation reading value
		my $cur_val = ReadingsVal($act_dev, $currentActReading, 0);; # current (last known) actuation
		# remember all parsed values
		$T[$i++] = { 
			'req_name' => $req_name, 
			'act_dev'  => $act_dev, 
			'weight'   => $weight,
			'req_val'  => $req_val,
			'cur_val'  => $cur_val};;
		# update sums
		$sum_req += $weight*$req_val;;
		$sum_cur += $weight*$cur_val;;
		Log 5, "CENTRALA_NOTIFY: $req_name=$weight*$req_val, cur=$weight*$cur_val";;
	};; 
	
	### we have the final sum
	fhem ("set CENTRALA sum_req $sum_req");; 
	fhem ("set CENTRALA sum_cur $sum_cur");; 
	
	### evaluate actual state
    my $boiler_req = ($sum_req > $boilerActTreshold) ;; # is boiler heating needed?
    my $valves_cur_opened =  ($sum_cur > $boilerActTreshold) ;;  # are valves CURRENTLY opened enough?
		
    ### update VALVES (if necessary)
	#if ($boiler_req) { # independent relay (boilerDevice) 
	if ($boiler_req or $valves_cur_opened) {  # FHT relay (boiler listening to valves radio messages) 
		# ask for an update of valves actuations
		for my $t (@T) {
			Log 1, "CENTRALA_NOTIFY: (VALVE) $t->{'req_name'}=$t->{'weight'}*$t->{'req_val'}, current=$t->{'weight'}*$t->{'cur_val'}, <set $t->{'act_dev'} $setActReading $t->{'req_val'}>";;
			fhem ("set $t->{'act_dev'} $setActReading $t->{'req_val'}");;
		}
	 } else {
		Log 1, "CENTRALA_NOTIFY: (VALVES) IDLE";;	 
	 };;
	
	###  update BOILER
	if ($boiler_req) {
	    if( $valves_cur_opened) {
			# turn boiler ON
			fhem "set $boilerDevice $boilerSetReading on";;
			Log 1, "CENTRALA_NOTIFY: $boilerDevice ON (req=$sum_req, cur=$sum_cur, lim=$boilerActTreshold)";;
		}  else { 
			# cannot heat when valves are not opened enough! 
			fhem "set $boilerDevice $boilerSetReading off";;
			Log 1, "CENTRALA_NOTIFY: $boilerDevice OFF_HeatReq (req=$sum_req, cur=$sum_cur, lim=$boilerActTreshold)";;
		}
	} else {
		# boiler OFF since heating not needed
		fhem "set $boilerDevice $boilerSetReading off";;
		Log 1, "CENTRALA_NOTIFY: $boilerDevice OFF_NoHeat (req=$sum_req, cur=$sum_cur, lim=$boilerActTreshold)";;
	} ;;
}