#include "game.h"
#include "trap.h"
#include "rng.h"

// A pit becomes less effective as it fills with corpses.
float pit_effectiveness(game *g, int x, int y)
{
    int corpse_volume = 0;
    for (int i = 0; i < g->m.i_at(x, y).size(); i++) {
        item &j = g->m.i_at(x, y)[i];
        if (j.type->id == "corpse") {
            corpse_volume += j.volume();
        }
    }

    int filled_volume = 75 * 10; // 10 zombies; see item::volume

    float eff = 1.0f - float(corpse_volume) / filled_volume;
    if (eff < 0) eff = 0;
    return eff;
}

void trapfunc::bubble(game *g, int x, int y)
{
 g->add_msg(_("You step on some bubble wrap!"));
 g->u.add_memorial_log(_("Stepped on bubble wrap."));
 g->sound(x, y, 18, _("Pop!"));
 g->m.remove_trap(x, y);
}

void trapfuncm::bubble(game *g, monster *z, int x, int y)
{
    // tiny animals don't trigger bubble wrap
    if (z->type->size == MS_TINY)
        return;

 g->sound(x, y, 18, _("Pop!"));
 g->m.remove_trap(x, y);
}

void trapfuncm::cot(game *g, monster *z, int x, int y)
{
 g->add_msg(_("The %s stumbles over the cot"), z->name().c_str());
 z->moves -= 100;
}

void trapfunc::beartrap(game *g, int x, int y)
{
 g->add_msg(_("A bear trap closes on your foot!"));
 g->u.add_memorial_log(_("Caught by a beartrap."));
 g->sound(x, y, 8, _("SNAP!"));
 g->u.hit(g, bp_legs, rng(0, 1), 10, 16);
 g->u.add_disease("beartrap", -1);
 g->m.remove_trap(x, y);
 g->m.spawn_item(x, y, "beartrap", g->turn);
}

void trapfuncm::beartrap(game *g, monster *z, int x, int y)
{
    // tiny animals don't trigger bear traps
    if (z->type->size == MS_TINY)
        return;

 g->sound(x, y, 8, _("SNAP!"));
 if (z->hurt(35)) {
  g->kill_mon(g->mon_at(x, y));
  g->m.spawn_item(x, y, "beartrap", 0);
 } else {
  z->moves = 0;
  z->add_effect(ME_BEARTRAP, rng(8, 15));
 }
 g->m.remove_trap(x, y);
 item beartrap(g->itypes["beartrap"], 0);
 z->add_item(beartrap);
}

void trapfunc::board(game *g, int x, int y)
{
 g->add_msg(_("You step on a spiked board!"));
 g->u.add_memorial_log(_("Stepped on a spiked board."));
 g->u.hit(g, bp_feet, 0, 0, rng(6, 10));
 g->u.hit(g, bp_feet, 1, 0, rng(6, 10));
}

void trapfuncm::board(game *g, monster *z, int x, int y)
{
    // tiny animals don't trigger spiked boards, they can squeeze between the nails
    if (z->type->size == MS_TINY)
        return;

 if (g->u_see(z))
  g->add_msg(_("The %s steps on a spiked board!"), z->name().c_str());
 if (z->hurt(rng(6, 10)))
  g->kill_mon(g->mon_at(x, y));
 else
  z->moves -= 80;
}

void trapfunc::tripwire(game *g, int x, int y)
{
 g->add_msg(_("You trip over a tripwire!"));
 g->u.add_memorial_log(_("Tripped on a tripwire."));
 std::vector<point> valid;
 for (int j = x - 1; j <= x + 1; j++) {
  for (int k = y - 1; k <= y + 1; k++) {
   if (g->is_empty(j, k)) // No monster, NPC, or player, plus valid for movement
    valid.push_back(point(j, k));
  }
 }
 if (!valid.empty()) {
  int index = rng(0, valid.size() - 1);
  g->u.posx = valid[index].x;
  g->u.posy = valid[index].y;
 }
 g->u.moves -= 150;
 if (rng(5, 20) > g->u.dex_cur)
  g->u.hurtall(rng(1, 4));
}

void trapfuncm::tripwire(game *g, monster *z, int x, int y)
{
    // tiny animals don't trigger tripwires, they just squeeze under it
    if (z->type->size == MS_TINY)
        return;

 if (g->u_see(z))
  g->add_msg(_("The %s trips over a tripwire!"), z->name().c_str());
 z->stumble(g, false);
 if (rng(0, 10) > z->type->sk_dodge && z->hurt(rng(1, 4)))
  g->kill_mon(g->mon_at(z->posx(), z->posy()));
}

void trapfunc::crossbow(game *g, int x, int y)
{
 bool add_bolt = true;
 g->add_msg(_("You trigger a crossbow trap!"));
 g->u.add_memorial_log(_("Triggered a crossbow trap."));
 if (!one_in(4) && rng(8, 20) > g->u.dodge(g)) {
  body_part hit = num_bp;
  switch (rng(1, 10)) {
   case  1: hit = bp_feet; break;
   case  2:
   case  3:
   case  4: hit = bp_legs; break;
   case  5:
   case  6:
   case  7:
   case  8:
   case  9: hit = bp_torso; break;
   case 10: hit = bp_head; break;
  }
  int side = rng(0, 1);
  g->add_msg(_("Your %s is hit!"), body_part_name(hit, side).c_str());
  g->u.hit(g, hit, side, 0, rng(20, 30));
  add_bolt = !one_in(10);
 } else
  g->add_msg(_("You dodge the shot!"));
 g->m.remove_trap(x, y);
 g->m.spawn_item(x, y, "crossbow", 0);
 g->m.spawn_item(x, y, "string_6", 0);
 if (add_bolt)
  g->m.spawn_item(x, y, "bolt_steel", 0, 0, 1);
}

void trapfuncm::crossbow(game *g, monster *z, int x, int y)
{
    bool add_bolt = true;
    bool seen = g->u_see(z);
    int chance = 0;
    // adapted from shotgun code - chance of getting hit depends on size
    switch (z->type->size)
    {
        case MS_TINY:   chance = 50; break;
        case MS_SMALL:  chance =  8; break;
        case MS_MEDIUM: chance =  6; break;
        case MS_LARGE:  chance =  4; break;
        case MS_HUGE:   chance =  1; break;
    }

    if (one_in(chance))
    {
        if (seen)
            g->add_msg(_("A bolt shoots out and hits the %s!"), z->name().c_str());
        if (z->hurt(rng(20, 30)))
            g->kill_mon(g->mon_at(x, y));
        add_bolt = !one_in(10);
    }
    else if (seen)
        g->add_msg(_("A bolt shoots out, but misses the %s."), z->name().c_str());
    g->m.remove_trap(x, y);
    g->m.spawn_item(x, y, "crossbow", 0);
    g->m.spawn_item(x, y, "string_6", 0);
    if (add_bolt)
    g->m.spawn_item(x, y, "bolt_steel", 0, 0, 1);
}

void trapfunc::shotgun(game *g, int x, int y)
{
 g->add_msg(_("You trigger a shotgun trap!"));
 g->u.add_memorial_log(_("Triggered a shotgun trap."));
 int shots = (one_in(8) || one_in(20 - g->u.str_max) ? 2 : 1);
 if (g->m.tr_at(x, y) == tr_shotgun_1)
  shots = 1;
 if (rng(5, 50) > g->u.dodge(g)) {
  body_part hit = num_bp;
  switch (rng(1, 10)) {
   case  1: hit = bp_feet; break;
   case  2:
   case  3:
   case  4: hit = bp_legs; break;
   case  5:
   case  6:
   case  7:
   case  8:
   case  9: hit = bp_torso; break;
   case 10: hit = bp_head; break;
  }
  int side = rng(0, 1);
  g->add_msg(_("Your %s is hit!"), body_part_name(hit, side).c_str());
  g->u.hit(g, hit, side, 0, rng(40 * shots, 60 * shots));
 } else
  g->add_msg(_("You dodge the shot!"));
 if (shots == 2 || g->m.tr_at(x, y) == tr_shotgun_1) {
  g->m.spawn_item(x, y, "shotgun_sawn", 0);
  g->m.spawn_item(x, y, "string_6", 0);
  g->m.remove_trap(x, y);
 } else
  g->m.add_trap(x, y, tr_shotgun_1);
}

void trapfuncm::shotgun(game *g, monster *z, int x, int y)
{
 bool seen = g->u_see(z);
 int chance = 0;
 switch (z->type->size) {
  case MS_TINY:   chance = 100; break;
  case MS_SMALL:  chance =  16; break;
  case MS_MEDIUM: chance =  12; break;
  case MS_LARGE:  chance =   8; break;
  case MS_HUGE:   chance =   2; break;
 }
 int shots = (one_in(8) || one_in(chance) ? 2 : 1);
 if (g->m.tr_at(x, y) == tr_shotgun_1)
  shots = 1;
 if (seen)
  g->add_msg(_("A shotgun fires and hits the %s!"), z->name().c_str());
 if (z->hurt(rng(40 * shots, 60 * shots)))
  g->kill_mon(g->mon_at(x, y));
 if (shots == 2 || g->m.tr_at(x, y) == tr_shotgun_1) {
  g->m.remove_trap(x, y);
  g->m.spawn_item(x, y, "shotgun_sawn", 0);
  g->m.spawn_item(x, y, "string_6", 0);
  g->m.spawn_item(x, y, "shotgun_sawn", 0);
  g->m.spawn_item(x, y, "string_6", 0);
 } else
  g->m.add_trap(x, y, tr_shotgun_1);
}


void trapfunc::blade(game *g, int x, int y)
{
 g->add_msg(_("A blade swings out and hacks your torso!"));
 g->u.add_memorial_log(_("Triggered a blade trap."));
 g->u.hit(g, bp_torso, 0, 12, 30);
}

void trapfuncm::blade(game *g, monster *z, int x, int y)
{
 if (g->u_see(z))
  g->add_msg(_("A blade swings out and hacks the %s!"), z->name().c_str());
 int cutdam = 30 - z->armor_cut();
 int bashdam = 12 - z->armor_bash();
 if (cutdam < 0)
  cutdam = 0;
 if (bashdam < 0)
  bashdam = 0;
 if (z->hurt(bashdam + cutdam))
  g->kill_mon(g->mon_at(x, y));
}

void trapfunc::snare_light(game *g, int x, int y)
{
 g->sound(x, y, 2, _("Snap!"));
 g->add_msg(_("A snare closes on your leg."));
 g->u.add_memorial_log(_("Triggered a light snare."));
 g->u.add_disease("lightsnare", rng(10, 20));
 g->m.remove_trap(x, y);
 g->m.spawn_item(x, y, "string_36", 0);
 g->m.spawn_item(x, y, "snare_trigger", 0);
}

void trapfuncm::snare_light(game *g, monster *z, int x, int y)
{
 bool seen = g->u_see(z);
 g->sound(x, y, 2, _("Snap!"));
 switch (z->type->size) {
  case MS_TINY:
   if(seen){
    g->add_msg(_("The %s has been snared!"), z->name().c_str());
   }
   if(z->hurt(10)){
    g->kill_mon(g->mon_at(x, y));
   } else {
    z->add_effect(ME_BEARTRAP, -1);
   }
   break;
  case MS_SMALL:
   if(seen){
    g->add_msg(_("The %s has been snared!"), z->name().c_str());
   }
   z->moves = 0;
   z->add_effect(ME_BEARTRAP, rng(100, 150));
   break;
  case MS_MEDIUM:
   if(seen){
    g->add_msg(_("The %s has been snared!"), z->name().c_str());
   }
   z->moves = 0;
   z->add_effect(ME_BEARTRAP, rng(20, 30));
   break;
  case MS_LARGE:
   if(seen){
    g->add_msg(_("The snare has no effect on the %s!"), z->name().c_str());
   }
   break;
  case MS_HUGE:
   if(seen){
    g->add_msg(_("The snare has no effect on the %s!"), z->name().c_str());
   }
   break;
 }
 g->m.remove_trap(x, y);
 g->m.spawn_item(x, y, "string_36", 0);
 g->m.spawn_item(x, y, "snare_trigger", 0);
}

void trapfunc::snare_heavy(game *g, int x, int y)
{
 int side = one_in(2) ? 0 : 1;
 body_part hit = bp_legs;
 g->sound(x, y, 4, _("Snap!"));
 g->add_msg(_("A snare closes on your %s."), body_part_name(hit, side).c_str());
 g->u.add_memorial_log(_("Triggered a heavy snare."));
 g->u.hit(g, bp_legs, side, 15, 20);
 g->u.add_disease("heavysnare", rng(20, 30));
 g->m.remove_trap(x, y);
 g->m.spawn_item(x, y, "rope_6", 0);
 g->m.spawn_item(x, y, "snare_trigger", 0);
}

void trapfuncm::snare_heavy(game *g, monster *z, int x, int y)
{
 bool seen = g->u_see(z);
 g->sound(x, y, 4, _("Snap!"));
 switch (z->type->size) {
  case MS_TINY:
   if(seen){
    g->add_msg(_("The %s has been snared!"), z->name().c_str());
   }
   if(z->hurt(20)){
    g->kill_mon(g->mon_at(x, y));
   } else {
    z->moves = 0;
    z->add_effect(ME_BEARTRAP, -1);
   }
   break;
  case MS_SMALL:
   if(seen){
    g->add_msg(_("The %s has been snared!"), z->name().c_str());
   }
   if(z->hurt(20)){
    g->kill_mon(g->mon_at(x, y));
   } else {
    z->moves = 0;
    z->add_effect(ME_BEARTRAP, -1);
   }
   break;
  case MS_MEDIUM:
   if(seen){
    g->add_msg(_("The %s has been snared!"), z->name().c_str());
   }
   if(z->hurt(10)){
    g->kill_mon(g->mon_at(x, y));
   } else {
    z->moves = 0;
    z->add_effect(ME_BEARTRAP, rng(100, 150));
   }
   break;
  case MS_LARGE:
   if(seen){
    g->add_msg(_("The %s has been snared!"), z->name().c_str());
   }
    z->moves = 0;
    z->add_effect(ME_BEARTRAP, rng(20, 30));
   break;
  case MS_HUGE:
   if(seen){
    g->add_msg(_("The snare has no effect on the %s!"), z->name().c_str());
   }
   break;
 }
 g->m.remove_trap(x, y);
 g->m.spawn_item(x, y, "snare_trigger", 0);
 g->m.spawn_item(x, y, "rope_6", 0);
}

void trapfunc::landmine(game *g, int x, int y)
{
 g->add_msg(_("You trigger a land mine!"));
 g->u.add_memorial_log(_("Stepped on a land mine."));
 g->explosion(x, y, 10, 8, false);
 g->m.remove_trap(x, y);
}

void trapfuncm::landmine(game *g, monster *z, int x, int y)
{
    // tiny animals are too light to trigger land mines
    if (z->type->size == MS_TINY)
        return;

 if (g->u_see(x, y))
  g->add_msg(_("The %s steps on a land mine!"), z->name().c_str());
 g->explosion(x, y, 10, 8, false);
 g->m.remove_trap(x, y);
}

void trapfunc::boobytrap(game *g, int x, int y)
{
 g->add_msg(_("You trigger a booby trap!"));
 g->u.add_memorial_log(_("Triggered a booby trap."));
 g->explosion(x, y, 18, 12, false);
 g->m.remove_trap(x, y);
}

void trapfuncm::boobytrap(game *g, monster *z, int x, int y)
{
 if (g->u_see(x, y))
  g->add_msg(_("The %s triggers a booby trap!"), z->name().c_str());
 g->explosion(x, y, 18, 12, false);
 g->m.remove_trap(x, y);
}

void trapfunc::telepad(game *g, int x, int y)
{
    //~ the sound of a telepad functioning
    g->sound(x, y, 6, _("vvrrrRRMM*POP!*"));
    g->add_msg(_("The air shimmers around you..."));
    g->u.add_memorial_log(_("Triggered a teleport trap."));
    g->teleport();
}

void trapfuncm::telepad(game *g, monster *z, int x, int y)
{
 g->sound(x, y, 6, _("vvrrrRRMM*POP!*"));
 if (g->u_see(z))
  g->add_msg(_("The air shimmers around the %s..."), z->name().c_str());

 int tries = 0;
 int newposx, newposy;
 do {
  newposx = rng(z->posx() - SEEX, z->posx() + SEEX);
  newposy = rng(z->posy() - SEEY, z->posy() + SEEY);
  tries++;
 } while (g->m.move_cost(newposx, newposy) == 0 && tries != 10);

 if (tries == 10)
  g->explode_mon(g->mon_at(z->posx(), z->posy()));
 else {
  int mon_hit = g->mon_at(newposx, newposy);
  if (mon_hit != -1) {
   if (g->u_see(z))
    g->add_msg(_("The %s teleports into a %s, killing them both!"),
               z->name().c_str(), g->zombie(mon_hit).name().c_str());
   g->explode_mon(mon_hit);
  } else {
   z->setpos(newposx, newposy);
  }
 }
}

void trapfunc::goo(game *g, int x, int y)
{
 g->add_msg(_("You step in a puddle of thick goo."));
 g->u.add_memorial_log(_("Stepped into thick goo."));
 g->u.infect("slimed", bp_feet, 6, 20, g);
 if (one_in(3)) {
  g->add_msg(_("The acidic goo eats away at your feet."));
  g->u.hit(g, bp_feet, 0, 0, 5);
  g->u.hit(g, bp_feet, 1, 0, 5);
 }
 g->m.remove_trap(x, y);
}

void trapfuncm::goo(game *g, monster *z, int x, int y)
{
 if (z->type->id == mon_blob) {
  z->speed += 15;
  z->hp = z->speed;
 } else {
  z->poly(g->mtypes[mon_blob]);
  z->speed -= 15;
  z->hp = z->speed;
 }
 g->m.remove_trap(x, y);
}

void trapfunc::dissector(game *g, int x, int y)
{
 g->add_msg(_("Electrical beams emit from the floor and slice your flesh!"));
 g->u.add_memorial_log(_("Stepped into a dissector."));
 //~ the sound of a dissector dissecting
 g->sound(x, y, 10, _("BRZZZAP!"));
 g->u.hit(g, bp_head,  0, 0, 15);
 g->u.hit(g, bp_torso, 0, 0, 20);
 g->u.hit(g, bp_arms,  0, 0, 12);
 g->u.hit(g, bp_arms,  1, 0, 12);
 g->u.hit(g, bp_hands, 0, 0, 10);
 g->u.hit(g, bp_hands, 1, 0, 10);
 g->u.hit(g, bp_legs,  0, 0, 12);
 g->u.hit(g, bp_legs,  1, 0, 12);
 g->u.hit(g, bp_feet,  0, 0, 10);
 g->u.hit(g, bp_feet,  1, 0, 10);
}

void trapfuncm::dissector(game *g, monster *z, int x, int y)
{
 g->sound(x, y, 10, _("BRZZZAP!"));
 if (z->hurt(60))
  g->explode_mon(g->mon_at(x, y));
}

void trapfunc::pit(game *g, int x, int y)
{
    g->add_msg(_("You fall in a pit!"));
    g->u.add_memorial_log(_("Fell in a pit."));
    if (g->u.has_trait("WINGS_BIRD")) {
        g->add_msg(_("You flap your wings and flutter down gracefully."));
    } else {
        float eff = pit_effectiveness(g, x, y);
        int dodge = g->u.dodge(g);
        int damage = eff * rng(10, 20) - rng(dodge, dodge * 5);
        if (damage > 0) {
            g->add_msg(_("You hurt yourself!"));
            g->u.hurtall(rng(int(damage / 2), damage));
            g->u.hit(g, bp_legs, 0, damage, 0);
            g->u.hit(g, bp_legs, 1, damage, 0);
        } else {
            g->add_msg(_("You land nimbly."));
        }
    }
    g->u.add_disease("in_pit", -1);
}

void trapfuncm::pit(game *g, monster *z, int x, int y)
{
    // tiny animals aren't hurt by falling into pits
    if (z->type->size == MS_TINY)
        return;

    if (g->u_see(x, y)) {
        g->add_msg(_("The %s falls in a pit!"), z->name().c_str());
    }

    if (z->hurt(pit_effectiveness(g, x, y) * rng(10, 20))) {
        g->kill_mon(g->mon_at(x, y));
    } else {
        z->moves = -1000;
    }
}

void trapfunc::pit_spikes(game *g, int x, int y)
{
    g->add_msg(_("You fall in a pit!"));
    g->u.add_memorial_log(_("Fell into a spiked pit."));
    int dodge = g->u.dodge(g);
    int damage = pit_effectiveness(g, x, y) * rng(20, 50);
    if (g->u.has_trait("WINGS_BIRD")) {
        g->add_msg(_("You flap your wings and flutter down gracefully."));
    } else if (0 == damage || rng(5, 30) < dodge) {
        g->add_msg(_("You avoid the spikes within."));
    } else {
        body_part hit = num_bp;
        switch (rng(1, 10)) {
            case  1:
            case  2: hit = bp_legs; break;
            case  3:
            case  4: hit = bp_arms; break;
            case  5:
            case  6:
            case  7:
            case  8:
            case  9:
            case 10: hit = bp_torso; break;
        }
        int side = rng(0, 1);
        g->add_msg(_("The spikes impale your %s!"), body_part_name(hit, side).c_str());
        g->u.hit(g, hit, side, 0, damage);
        if (one_in(4)) {
            g->add_msg(_("The spears break!"));
            g->m.ter_set(x, y, t_pit);
            g->m.add_trap(x, y, tr_pit);
            for (int i = 0; i < 4; i++) { // 4 spears to a pit
                if (one_in(3)) {
                    g->m.spawn_item(x, y, "spear_wood", g->turn);
                }
            }
        }
    }
    g->u.add_disease("in_pit", -1);
}

void trapfuncm::pit_spikes(game *g, monster *z, int x, int y)
{
    // tiny animals aren't hurt by falling into spiked pits
    if (z->type->size == MS_TINY) return;

    bool sees = g->u_see(z);
    if (sees) {
        g->add_msg(_("The %s falls in a spiked pit!"), z->name().c_str());
    }

    if (z->hurt(rng(20, 50))) {
        g->kill_mon(g->mon_at(x, y));
    } else {
        z->moves = -1000;
    }

    if (one_in(4)) {
        if (sees) {
            g->add_msg(_("The spears break!"));
        }
        g->m.ter_set(x, y, t_pit);
        g->m.add_trap(x, y, tr_pit);
        for (int i = 0; i < 4; i++) { // 4 spears to a pit
            if (one_in(3)) {
                g->m.spawn_item(x, y, "spear_wood", g->turn);
            }
        }
    }
}

void trapfunc::lava(game *g, int x, int y)
{
 g->add_msg(_("The %s burns you horribly!"), g->m.tername(x, y).c_str());
 g->u.add_memorial_log(_("Stepped into lava."));
 g->u.hit(g, bp_feet, 0, 0, 20);
 g->u.hit(g, bp_feet, 1, 0, 20);
 g->u.hit(g, bp_legs, 0, 0, 20);
 g->u.hit(g, bp_legs, 1, 0, 20);
}


// MATERIALS-TODO: use fire resistance
void trapfuncm::lava(game *g, monster *z, int x, int y)
{
 bool sees = g->u_see(z);
 if (sees)
  g->add_msg(_("The %s burns the %s!"), g->m.tername(x, y).c_str(),
                                     z->name().c_str());

 int dam = 30;
 if (z->made_of("flesh"))
  dam = 50;
 if (z->made_of("veggy"))
  dam = 80;
 if (z->made_of("paper") || z->made_of(LIQUID) || z->made_of("powder") ||
     z->made_of("wood")  || z->made_of("cotton") || z->made_of("wool"))
  dam = 200;
 if (z->made_of("stone"))
  dam = 15;
 if (z->made_of("kevlar") || z->made_of("steel"))
  dam = 5;

 z->hurt(dam);
}


void trapfunc::sinkhole(game *g, int x, int y)
{
 g->add_msg(_("You step into a sinkhole, and start to sink down!"));
 g->u.add_memorial_log(_("Stepped into a sinkhole."));
 if (g->u.has_amount("rope_30", 1) &&
     query_yn(_("Throw your rope to try to catch something?"))) {
  int throwroll = rng(g->u.skillLevel("throw"),
                      g->u.skillLevel("throw") + g->u.str_cur + g->u.dex_cur);
  if (throwroll >= 12) {
   g->add_msg(_("The rope catches something!"));
   if (rng(g->u.skillLevel("unarmed"),
           g->u.skillLevel("unarmed") + g->u.str_cur) > 6) {
// Determine safe places for the character to get pulled to
    std::vector<point> safe;
    for (int i = g->u.posx - 1; i <= g->u.posx + 1; i++) {
     for (int j = g->u.posx - 1; j <= g->u.posx + 1; j++) {
      if (g->m.move_cost(i, j) > 0 && g->m.tr_at(i, j) != tr_pit)
       safe.push_back(point(i, j));
     }
    }
    if (safe.empty()) {
     g->add_msg(_("There's nowhere to pull yourself to, and you sink!"));
     g->u.use_amount("rope_30", 1);
     g->m.spawn_item(g->u.posx + rng(-1, 1), g->u.posy + rng(-1, 1),
                     "rope_30", g->turn);
     g->m.add_trap(g->u.posx, g->u.posy, tr_pit);
     g->vertical_move(-1, true);
    } else {
     g->add_msg(_("You pull yourself to safety!  The sinkhole collapses."));
     int index = rng(0, safe.size() - 1);
     g->u.posx = safe[index].x;
     g->u.posy = safe[index].y;
     g->update_map(g->u.posx, g->u.posy);
     g->m.add_trap(g->u.posx, g->u.posy, tr_pit);
    }
   } else {
    g->add_msg(_("You're not strong enough to pull yourself out..."));
    g->u.moves -= 100;
    g->u.use_amount("rope_30", 1);
    g->m.spawn_item(g->u.posx + rng(-1, 1), g->u.posy + rng(-1, 1),
                    "rope_30", g->turn);
    g->vertical_move(-1, true);
   }
  } else {
   g->add_msg(_("Your throw misses completely, and you sink!"));
   if (one_in((g->u.str_cur + g->u.dex_cur) / 3)) {
    g->u.use_amount("rope_30", 1);
    g->m.spawn_item(g->u.posx + rng(-1, 1), g->u.posy + rng(-1, 1),
                    "rope_30", g->turn);
   }
   g->m.add_trap(g->u.posx, g->u.posy, tr_pit);
   g->vertical_move(-1, true);
  }
 } else {
  g->add_msg(_("You sink into the sinkhole!"));
  g->vertical_move(-1, true);
 }
}

void trapfunc::ledge(game *g, int x, int y)
{
 g->add_msg(_("You fall down a level!"));
 g->u.add_memorial_log(_("Fell down a ledge."));
 g->vertical_move(-1, true);
}

void trapfuncm::ledge(game *g, monster *z, int x, int y)
{
 g->add_msg(_("The %s falls down a level!"), z->name().c_str());
 g->kill_mon(g->mon_at(x, y));
}

void trapfunc::temple_flood(game *g, int x, int y)
{
 g->add_msg(_("You step on a loose tile, and water starts to flood the room!"));
 g->u.add_memorial_log(_("Triggered a flood trap."));
 for (int i = 0; i < SEEX * MAPSIZE; i++) {
  for (int j = 0; j < SEEY * MAPSIZE; j++) {
   if (g->m.tr_at(i, j) == tr_temple_flood)
    g->m.remove_trap(i, j);
  }
 }
 g->add_event(EVENT_TEMPLE_FLOOD, g->turn + 3);
}

void trapfunc::temple_toggle(game *g, int x, int y)
{
 g->add_msg(_("You hear the grinding of shifting rock."));
 ter_id type = g->m.ter(x, y);
 for (int i = 0; i < SEEX * MAPSIZE; i++) {
  for (int j = 0; j < SEEY * MAPSIZE; j++) {
   switch (type) {
    case t_floor_red:
     if (g->m.ter(i, j) == t_rock_green)
      g->m.ter_set(i, j, t_floor_green);
     else if (g->m.ter(i, j) == t_floor_green)
      g->m.ter_set(i, j, t_rock_green);
     break;

    case t_floor_green:
     if (g->m.ter(i, j) == t_rock_blue)
      g->m.ter_set(i, j, t_floor_blue);
     else if (g->m.ter(i, j) == t_floor_blue)
      g->m.ter_set(i, j, t_rock_blue);
     break;

    case t_floor_blue:
     if (g->m.ter(i, j) == t_rock_red)
      g->m.ter_set(i, j, t_floor_red);
     else if (g->m.ter(i, j) == t_floor_red)
      g->m.ter_set(i, j, t_rock_red);
     break;

   }
  }
 }
}

void trapfunc::glow(game *g, int x, int y)
{
 if (one_in(3)) {
  g->add_msg(_("You're bathed in radiation!"));
  g->u.radiation += rng(10, 30);
 } else if (one_in(4)) {
  g->add_msg(_("A blinding flash strikes you!"));
  g->flashbang(g->u.posx, g->u.posy);
 } else
  g->add_msg(_("Small flashes surround you."));
}

void trapfuncm::glow(game *g, monster *z, int x, int y)
{
 if (one_in(3)) {
  z->hurt( rng(5, 10) );
  z->speed *= .9;
 }
}

void trapfunc::hum(game *g, int x, int y)
{
    int volume = rng(1, 200);
    std::string sfx;
    if (volume <= 10) {
        //~ a quiet humming sound
        sfx = _("hrm");
    } else if (volume <= 50) {
        //~ a humming sound
        sfx = _("hrmmm");
    } else if (volume <= 100) {
        //~ a loud humming sound
        sfx = _("HRMMM");
    } else {
        //~ a very loud humming sound
        sfx = _("VRMMMMMM");
    }

    g->sound(x, y, volume, sfx);
}

void trapfuncm::hum(game *g, monster *z, int x, int y)
{
 int volume = rng(1, 200);
 std::string sfx;
 if (volume <= 10)
  sfx = _("hrm");
 else if (volume <= 50)
  sfx = _("hrmmm");
 else if (volume <= 100)
  sfx = _("HRMMM");
 else
  sfx = _("VRMMMMMM");

 if (volume >= 150)
  z->add_effect(ME_DEAF, volume - 140);

 g->sound(x, y, volume, sfx);
}

void trapfunc::shadow(game *g, int x, int y)
{
 g->u.add_memorial_log(_("Triggered a shadow trap."));
 monster spawned(g->mtypes[mon_shadow]);
 int tries = 0, monx, mony, junk;
 do {
  if (one_in(2)) {
   monx = rng(g->u.posx - 5, g->u.posx + 5);
   mony = (one_in(2) ? g->u.posy - 5 : g->u.posy + 5);
  } else {
   monx = (one_in(2) ? g->u.posx - 5 : g->u.posx + 5);
   mony = rng(g->u.posy - 5, g->u.posy + 5);
  }
 } while (tries < 5 && !g->is_empty(monx, mony) &&
          !g->m.sees(monx, mony, g->u.posx, g->u.posy, 10, junk));

 if (tries < 5) {
  g->add_msg(_("A shadow forms nearby."));
  spawned.sp_timeout = rng(2, 10);
  spawned.spawn(monx, mony);
  g->add_zombie(spawned);
  g->m.remove_trap(x, y);
 }
}

void trapfunc::drain(game *g, int x, int y)
{
 g->u.add_memorial_log(_("Triggered a life-draining trap."));
 g->add_msg(_("You feel your life force sapping away."));
 g->u.hurtall(1);
}

void trapfuncm::drain(game *g, monster *z, int x, int y)
{
 z->hurt(1);
}

void trapfunc::snake(game *g, int x, int y)
{
 g->u.add_memorial_log(_("Triggered a shadow snake trap."));
 if (one_in(3)) {
  monster spawned(g->mtypes[mon_shadow_snake]);
  int tries = 0, monx, mony, junk;
  do {
   if (one_in(2)) {
    monx = rng(g->u.posx - 5, g->u.posx + 5);
    mony = (one_in(2) ? g->u.posy - 5 : g->u.posy + 5);
   } else {
    monx = (one_in(2) ? g->u.posx - 5 : g->u.posx + 5);
    mony = rng(g->u.posy - 5, g->u.posy + 5);
   }
  } while (tries < 5 && !g->is_empty(monx, mony) &&
           !g->m.sees(monx, mony, g->u.posx, g->u.posy, 10, junk));

  if (tries < 5) {
   g->add_msg(_("A shadowy snake forms nearby."));
   spawned.spawn(monx, mony);
   g->add_zombie(spawned);
   g->m.remove_trap(x, y);
   return;
  }
 }
 //~ the sound a snake makes
 g->sound(x, y, 10, _("ssssssss"));
 if (one_in(6))
  g->m.remove_trap(x, y);
}

void trapfuncm::snake(game *g, monster *z, int x, int y)
{
    g->sound(x, y, 10, _("ssssssss"));
    if (one_in(6)) {
        g->m.remove_trap(x, y);
    }
}

bool trap::is_benign()
{
    return (id == tr_rollmat || id == tr_cot || id == tr_brazier || id == tr_funnel || id == tr_makeshift_funnel);
}

/**
 * Given the id of a trap (without the tr_ prefix) returns the matching trap_id.
 * If no match is found, returns tr_null but prints a debug message.
 * @param trap_name The name of the trap to look up.
 * @return The matching trap_id constant.
 */
trap_id trap_id_from_string(std::string trap_name) {
  if ("null" == trap_name) {
    return tr_null;
  }
  if ("bubblewrap" == trap_name) {
    return tr_bubblewrap;
  }
  if ("cot" == trap_name) {
    return tr_cot;
  }
  if ("brazier" == trap_name) {
    return tr_brazier;
  }
  if ("funnel" == trap_name) {
    return tr_funnel;
  }
  if ("makeshift_funnel" == trap_name) {
    return tr_makeshift_funnel;
  }
  if ("rollmat" == trap_name) {
    return tr_rollmat;
  }
  if ("beartrap" == trap_name) {
    return tr_beartrap;
  }
  if ("beartrap_buried" == trap_name) {
    return tr_beartrap_buried;
  }
  if ("snare" == trap_name) {
    return tr_snare;
  }
  if ("nailboard" == trap_name) {
    return tr_nailboard;
  }
  if ("tripwire" == trap_name) {
    return tr_tripwire;
  }
  if ("crossbow" == trap_name) {
    return tr_crossbow;
  }
  if ("shotgun_2" == trap_name) {
    return tr_shotgun_2;
  }
  if ("shotgun_1" == trap_name) {
    return tr_shotgun_1;
  }
  if ("engine" == trap_name) {
    return tr_engine;
  }
  if ("blade" == trap_name) {
    return tr_blade;
  }
  if ("light_snare" == trap_name) {
    return tr_light_snare;
  }
  if ("heavy_snare" == trap_name) {
    return tr_heavy_snare;
  }
  if ("landmine" == trap_name) {
    return tr_landmine;
  }
  if ("landmine_buried" == trap_name) {
    return tr_landmine_buried;
  }
  if ("telepad" == trap_name) {
    return tr_telepad;
  }
  if ("goo" == trap_name) {
    return tr_goo;
  }
  if ("dissector" == trap_name) {
    return tr_dissector;
  }
  if ("sinkhole" == trap_name) {
    return tr_sinkhole;
  }
  if ("pit" == trap_name) {
    return tr_pit;
  }
  if ("spike_pit" == trap_name) {
    return tr_spike_pit;
  }
  if ("lava" == trap_name) {
    return tr_lava;
  }
  if ("portal" == trap_name) {
    return tr_portal;
  }
  if ("ledge" == trap_name) {
    return tr_ledge;
  }
  if ("boobytrap" == trap_name) {
    return tr_boobytrap;
  }
  if ("temple_flood" == trap_name) {
    return tr_temple_flood;
  }
  if ("temple_toggle" == trap_name) {
    return tr_temple_toggle;
  }
  if ("glow" == trap_name) {
    return tr_glow;
  }
  if ("hum" == trap_name) {
    return tr_hum;
  }
  if ("shadow" == trap_name) {
    return tr_shadow;
  }
  if ("drain" == trap_name) {
    return tr_drain;
  }
  if ("snake" == trap_name) {
    return tr_snake;
  }

  //No match
  debugmsg("No match found for trap '%s'.", trap_name.c_str());
  return tr_null;
}
