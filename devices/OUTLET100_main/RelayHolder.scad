outlineX = 75; // mm
outlineY = 50; // mm
holeDiameter = 3;   // mm
holeRadius = holeDiameter / 2;
pegRadius = holeRadius * 0.9;
holeCenterInset = 2.5;  // mm

baseInset = 1.5;    // mm
baseHeight = 3.5; // mm
plateHeight = 0.5;  // mm

wrapperAnnularRing = 2; // mm
wrapperHeight = 1.5;    // mm
wrapperInset = 0.2; // mm

samdHoleDiameter = 3;   // mm
samdHoleRadius = samdHoleDiameter / 2;
samdPegRadius = samdHoleRadius * 0.9;
samdPegHeight = 1.5;    // mm
samdZ = 31; // mm
samdX = 45; // mm
samdHoleToHole = 25;    // mm
samdStartHeight = 15;   // mm
samdArmThickness = 2;   // mm

// Do the base
translate([0, 0, baseHeight/2])
{
    difference()
    {
        cube([outlineX + wrapperAnnularRing*2, outlineY + wrapperAnnularRing*2, baseHeight], center=true);
        cube([outlineX - baseInset*2, outlineY - baseInset*2, baseHeight + 1], center=true);
    }
}

// Do the plate
translate([0, 0, -plateHeight/2])
{
   cube([outlineX + wrapperAnnularRing*2, outlineY + wrapperAnnularRing*2, plateHeight], center=true);
}

// Do the annular ring
translate([0, 0, (baseHeight + wrapperHeight)/2])
{
    difference()
    {
        cube([outlineX + wrapperAnnularRing*2, outlineY + wrapperAnnularRing*2, baseHeight + wrapperHeight], center=true);
        cube([outlineX + wrapperInset*2, outlineY + wrapperInset*2, baseHeight + wrapperHeight + 1], center=true);
    }
}

// Do the pegs
translate([(outlineX/2 - holeCenterInset), (outlineY/2 - holeCenterInset), 0])
{
    cylinder(h=baseHeight + wrapperHeight, r=pegRadius, $fn=90);
    cylinder(h=baseHeight, r=holeCenterInset, $fn=90);
}

translate([-(outlineX/2 - holeCenterInset), (outlineY/2 - holeCenterInset), 0])
{
    cylinder(h=baseHeight + wrapperHeight, r=pegRadius, $fn=90);
    cylinder(h=baseHeight, r=holeCenterInset, $fn=90);
}

translate([(outlineX/2 - holeCenterInset), -(outlineY/2 - holeCenterInset), 0])
{
    cylinder(h=baseHeight + wrapperHeight, r=pegRadius, $fn=90);
    cylinder(h=baseHeight, r=holeCenterInset, $fn=90);
}

translate([-(outlineX/2 - holeCenterInset), -(outlineY/2 - holeCenterInset), 0])
{
    cylinder(h=baseHeight + wrapperHeight, r=pegRadius, $fn=90);
    cylinder(h=baseHeight, r=holeCenterInset, $fn=90);
}

// Do the samd rack
translate([outlineX/2 - samdX, outlineY/2 + wrapperAnnularRing, -plateHeight])
{
    rotate([90, 0, 0])
    {
        translate([-samdHoleDiameter/2, 0, -samdArmThickness])
        {
            cube([samdHoleDiameter, baseHeight + samdStartHeight + samdHoleToHole + samdHoleDiameter, samdArmThickness], center=false);
        }
        
        translate([0, baseHeight + samdStartHeight + samdHoleToHole + samdHoleRadius, 0])
        {
            rotate([180, 0, 0])
            {
                cylinder(h=samdPegHeight + samdArmThickness, r=samdPegRadius, $fn=90);
            }
        }
        
        translate([0, baseHeight + samdStartHeight + samdHoleRadius, 0])
        {
            rotate([180, 0, 0])
            {
                cylinder(h=samdPegHeight + samdArmThickness, r=samdPegRadius, $fn=90);
            }
        }
    }
}
