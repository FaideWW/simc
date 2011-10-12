// ==========================================================================
// Dedmonwakeen's Raid DPS/TPS Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "simulationcraft.h"
#include <stack>

namespace { // ANONYMOUS NAMESPACE ==========================================

class xml_writer_t
{
private:
  FILE* file;
  enum state
  {
    NONE, TAG, TEXT
  };
  std::stack<std::string> current_tags;
  std::string tabulation;
  state current_state;
  std::string indentation;

  struct replacement { char from; const char* to; };

  static void replace_entity( std::string& str, char old_value, const char* new_value )
  {
    std::size_t len = strlen( new_value );
    std::string::size_type pos = 0;
    while ( ( pos = str.find( old_value, pos ) ) != str.npos )
    {
      str.replace( pos, 1, new_value );
      pos += len;
    }
  }

public:
  xml_writer_t( const std::string & filename )
    : tabulation( "    " ), current_state( NONE )
  {
    file = fopen( filename.c_str(), "w" );
  }

  ~xml_writer_t()
  {
    if( file )
    {
      fclose( file );
    }
  }

  bool ready() const { return file != NULL; }
  void set_tabulation( std::string & tabulation ) { this->tabulation = tabulation; }

  int printf( const char *format, ... ) const PRINTF_ATTRIBUTE( 2,3 )
  {
    va_list fmtargs;
    va_start( fmtargs, format );

    int retcode = vfprintf( file, format, fmtargs );

    va_end( fmtargs );

    return retcode;
  }

  void rebuild_indentation()
  {
    size_t size = current_tags.size();
    indentation.clear();
    for( size_t i = 0; i < size; i++ )
    {
      indentation.append( tabulation );
    }
  }

  void init_document( const std::string & stylesheet_file )
  {
    assert( current_state == NONE );

    printf( "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" );
    if( !stylesheet_file.empty() )
    {
      printf( "<?xml-stylesheet type=\"text/xml\" href=\"%s\"?>", stylesheet_file.c_str() );
    }

    current_state = TEXT;
  }

  void begin_tag( const std::string & tag )
  {
    assert( current_state != NONE );

    if( current_state != TEXT )
    {
      printf( ">" );
    }

    printf( "\n%s<%s", indentation.c_str(), tag.c_str() );

    current_tags.push( tag );
    rebuild_indentation();

    current_state = TAG;
  }

  void end_tag()
  {
    assert( current_state != NONE );

    std::string tag = current_tags.top();
    current_tags.pop();
    rebuild_indentation();

    if( current_state == TAG )
    {
      printf( "/>" );
    }
    else if( current_state == TEXT )
    {
      printf( "\n%s</%s>", indentation.c_str(), tag.c_str() );
    }

    current_state = TEXT;
  }

  void print_attribute( const std::string & name, const std::string & value )
  {
    assert( current_state != NONE );

    if( current_state == TAG )
    {
      printf( " %s=\"%s\"", name.c_str(), sanitize( value ).c_str() );
    }
  }

  void print_attribute_unescaped( const std::string & name, const std::string & value )
  {
    assert( current_state != NONE );

    if( current_state == TAG )
    {
      printf( " %s=\"%s\"", name.c_str(), value.c_str() );
    }
  }

  void print_tag( const std::string & name, const std::string & inner_value )
  {
    assert( current_state != NONE );

    if( current_state != TEXT )
    {
      printf( ">" );
    }

    printf( "\n%s<%s>%s</%s>", indentation.c_str(), name.c_str(), sanitize( inner_value ).c_str(), name.c_str() );

    current_state = TEXT;
  }

  void print_text( const std::string & input )
  {
    assert( current_state != NONE );

    if( current_state != TEXT )
    {
      printf( ">" );
    }

    printf( "\n%s", sanitize( input ).c_str() );

    current_state = TEXT;
  }

  static std::string sanitize( std::string v )
  {
    static const replacement replacements[] =
    {
      { '&', "&amp;" },
      { '"', "&quot;" },
      { '<', "&lt;" },
      { '>', "&gt;" },
    };

    for ( unsigned int i = 0; i < sizeof_array( replacements ); ++i )
      replace_entity( v, replacements[i].from, replacements[i].to );

    return v;
  }
};

// report_t::print_xml ======================================================

static const int PRINT_XML_PRECISION = 4;
static void print_xml_errors( sim_t* sim, xml_writer_t & writer );
static void print_xml_raid_events( sim_t* sim, xml_writer_t & writer );
static void print_xml_roster( sim_t* sim, xml_writer_t & writer );
static void print_xml_targets( sim_t* sim, xml_writer_t & writer );
static void print_xml_buffs( sim_t* sim, xml_writer_t & writer );
static void print_xml_hat_donors( sim_t* sim, xml_writer_t & writer );
static void print_xml_performance( sim_t* sim, xml_writer_t & writer );
static void print_xml_summary( sim_t* sim, xml_writer_t & writer );
static void print_xml_player( sim_t* sim, xml_writer_t & writer, player_t * p, player_t * owner );

static void print_xml_player_stats( xml_writer_t & writer, player_t * p );
static void print_xml_player_attribute( xml_writer_t & writer, const std::string& attribute, double initial, double gear, double buffed );
static void print_xml_player_actions( xml_writer_t & writer, player_t* p );
static void print_xml_player_buffs( xml_writer_t & writer, player_t * p );
static void print_xml_player_uptime( xml_writer_t & writer, player_t * p );
static void print_xml_player_procs( xml_writer_t & writer, player_t * p );
static void print_xml_player_gains( xml_writer_t & writer, player_t * p );
static void print_xml_player_scale_factors( xml_writer_t & writer, player_t * p );
static void print_xml_player_dps_plots( xml_writer_t & writer, player_t * p );
static void print_xml_player_charts( xml_writer_t & writer, player_t * p );

static void print_xml_errors( sim_t* sim, xml_writer_t & writer )
{
  size_t num_errors = sim -> error_list.size();
  if( num_errors > 0 )
  {
    writer.begin_tag( "errors" );
    for( size_t i=0; i < num_errors; i++ )
    {
      writer.begin_tag( "error" );
      writer.print_attribute( "message", sim -> error_list[ i ] );
      writer.end_tag(); // </error>
    }
    writer.end_tag(); // </errors>
  }
}

static void print_xml_raid_events( sim_t* sim, xml_writer_t & writer )
{
  if ( ! sim -> raid_events_str.empty() )
  {
    writer.begin_tag( "raid_events" );

    std::vector<std::string> raid_event_names;
    int num_raid_events = util_t::string_split( raid_event_names, sim -> raid_events_str, "/" );

    for ( int i=0; i < num_raid_events; i++ )
    {
      writer.begin_tag( "raid_event" );
      writer.print_attribute( "index", util_t::to_string( i ) );
      writer.print_attribute( "name", raid_event_names[i] );
      writer.end_tag(); // </raid_event>
    }

    writer.end_tag(); // </raid_events>
  }
}

static void print_xml_roster( sim_t* sim, xml_writer_t & writer )
{
  writer.begin_tag( "players" );

  int num_players = ( int ) sim -> players_by_dps.size();
  for ( int i = 0; i < num_players; ++i )
  {
    player_t * current_player = sim -> players_by_name[ i ];
    print_xml_player( sim, writer, current_player, NULL );
    for ( pet_t* pet = sim -> players_by_name[ i ] -> pet_list; pet; pet = pet -> next_pet )
    {
      if ( pet -> summoned )
        print_xml_player( sim, writer, pet, current_player );
    }
  }

  writer.end_tag(); // </players>
}

static void print_xml_targets( sim_t* sim, xml_writer_t & writer )
{
  writer.begin_tag( "targets" );

  size_t count = sim -> targets_by_name.size();
  for( size_t i = 0; i < count; ++i )
  {
    player_t * current_player = sim -> targets_by_name[ i ];
    print_xml_player( sim, writer, current_player, NULL );
    for ( pet_t* pet = current_player -> pet_list; pet; pet = pet -> next_pet )
    {
      if ( pet -> summoned )
        print_xml_player( sim, writer, pet, current_player );
    }
  }

  writer.end_tag(); // </targets>
}

static void print_xml_player( sim_t * sim, xml_writer_t & writer, player_t * p, player_t * owner )
{
  writer.begin_tag( "player" );
  writer.print_attribute( "name", p -> name() );
  if( owner )
    writer.print_attribute( "owner", owner -> name() );
  writer.print_tag( "target_type", p -> is_enemy() ? "Target" : p -> is_add() ? "Add" : "Player" );
  writer.print_tag( "level", util_t::to_string( p -> level ) );
  writer.print_tag( "race", p -> race_str.c_str() );
  writer.print_tag( "player_type", util_t::player_type_string( p->type ) );
  if( p -> is_pet() )
    writer.print_tag( "pet_type", util_t::pet_type_string( p -> cast_pet() -> pet_type ) );
  writer.print_tag( "talent_tree", util_t::talent_tree_string( p -> primary_tree() ) );
  writer.print_tag( "primary_role", util_t::role_type_string( p -> primary_role() ) );
  writer.print_tag( "position", p -> position_str );
  writer.begin_tag( "dps" );
  writer.print_attribute( "value", util_t::to_string( p -> dps.mean, PRINT_XML_PRECISION ) );
  writer.print_attribute( "effective", util_t::to_string( p -> dpse.mean, PRINT_XML_PRECISION ) );
  writer.print_attribute( "error", util_t::to_string( p -> dps_error, PRINT_XML_PRECISION ) );
  writer.print_attribute( "range", util_t::to_string( ( p -> dps.max - p -> dps.min ) / 2.0, PRINT_XML_PRECISION ) );
  writer.print_attribute( "convergence", util_t::to_string( p -> dps_convergence, PRINT_XML_PRECISION ) );
  writer.end_tag(); // </dps>

  if ( p -> rps_loss > 0 )
  {
    writer.begin_tag( "dpr" );
    writer.print_attribute( "value", util_t::to_string( p -> dpr, PRINT_XML_PRECISION ) );
    writer.print_attribute( "rps_loss", util_t::to_string( p -> rps_loss, PRINT_XML_PRECISION ) );
    writer.print_attribute( "rps_gain", util_t::to_string( p -> rps_gain, PRINT_XML_PRECISION ) );
    writer.print_attribute( "resource", util_t::resource_type_string( p -> primary_resource() ) );
    writer.end_tag(); // </dpr>
  }

  writer.print_tag( "waiting_time_pct", util_t::to_string( p -> fight_length.mean ? 100.0 * p -> waiting_time.mean / p -> fight_length.mean : 0, PRINT_XML_PRECISION ) );
  writer.print_tag( "apm", util_t::to_string( p -> fight_length.mean ? 60.0 * p -> executed_foreground_actions.mean / p -> fight_length.mean : 0, PRINT_XML_PRECISION ) );
  writer.print_tag( "active_time_pct", util_t::to_string( sim -> simulation_length.mean ? p -> fight_length.mean / sim -> simulation_length.mean * 100.0 : 0, PRINT_XML_PRECISION ) );

  if ( p -> origin_str.compare( "unknown" ) )
    writer.print_tag( "origin", p -> origin_str.c_str() );

  if ( ! p -> talents_str.empty() )
    writer.print_tag( "talents_url", p -> talents_str.c_str() );

  print_xml_player_stats( writer, p );
  print_xml_player_actions( writer, p );

  print_xml_player_buffs( writer, p );
  print_xml_player_uptime( writer, p );
  print_xml_player_procs( writer, p );
  print_xml_player_gains( writer, p );
  print_xml_player_scale_factors( writer, p );
  print_xml_player_dps_plots( writer, p );
  print_xml_player_charts( writer, p );

  writer.end_tag(); // </player>
}

static void print_xml_player_stats( xml_writer_t & writer, player_t * p )
{
  print_xml_player_attribute( writer, "strength",
                              p -> strength(),  p -> stats.attribute[ ATTR_STRENGTH  ], p -> attribute_buffed[ ATTR_STRENGTH  ] );
  print_xml_player_attribute( writer, "agility",
                              p -> agility(),   p -> stats.attribute[ ATTR_AGILITY   ], p -> attribute_buffed[ ATTR_AGILITY   ] );
  print_xml_player_attribute( writer, "stamina",
                              p -> stamina(),   p -> stats.attribute[ ATTR_STAMINA   ], p -> attribute_buffed[ ATTR_STAMINA   ] );
  print_xml_player_attribute( writer, "intellect",
                              p -> intellect(), p -> stats.attribute[ ATTR_INTELLECT ], p -> attribute_buffed[ ATTR_INTELLECT ] );
  print_xml_player_attribute( writer, "spirit",
                              p -> spirit(),    p -> stats.attribute[ ATTR_SPIRIT    ], p -> attribute_buffed[ ATTR_SPIRIT    ] );
  print_xml_player_attribute( writer, "mastery",
                              p -> composite_mastery(), p -> stats.mastery_rating, p -> buffed_mastery );
  print_xml_player_attribute( writer, "spellpower",
                              p -> composite_spell_power( SCHOOL_MAX ) * p -> composite_spell_power_multiplier(), p -> stats.spell_power, p -> buffed_spell_power );
  print_xml_player_attribute( writer, "spellhit",
                              100 * p -> composite_spell_hit(), p -> stats.hit_rating, 100 * p -> buffed_spell_hit );
  print_xml_player_attribute( writer, "spellcrit",
                              100 * p -> composite_spell_crit(), p -> stats.crit_rating, 100 * p -> buffed_spell_crit );
  print_xml_player_attribute( writer, "spellpenetration",
                              100 * p -> composite_spell_penetration(), p -> stats.spell_penetration, 100 * p -> buffed_spell_penetration );
  print_xml_player_attribute( writer, "spellhaste",
                              100 * ( 1 / p -> spell_haste - 1 ), p -> stats.haste_rating, 100 * ( 1 / p -> buffed_spell_haste - 1 ) );
  print_xml_player_attribute( writer, "mp5",
                              p -> composite_mp5(), p -> stats.mp5, p -> buffed_mp5 );
  print_xml_player_attribute( writer, "attackpower",
                              p -> composite_attack_power() * p -> composite_attack_power_multiplier(), p -> stats.attack_power, p -> buffed_attack_power );
  print_xml_player_attribute( writer, "attackhit",
                              100 * p -> composite_attack_hit(), p -> stats.hit_rating, 100 * p -> buffed_attack_hit );
  print_xml_player_attribute( writer, "attackcrit",
                              100 * p -> composite_attack_crit(), p -> stats.crit_rating, 100 * p -> buffed_attack_crit );
  print_xml_player_attribute( writer, "expertise",
                              100 * p -> composite_attack_expertise(), p -> stats.expertise_rating, 100 * p -> buffed_attack_expertise );
  print_xml_player_attribute( writer, "attackhaste",
                              100 * ( 1 / p -> composite_attack_haste() - 1 ), p -> stats.haste_rating, 100 * ( 1 / p -> buffed_attack_haste - 1 ) );
  print_xml_player_attribute( writer, "attackspeed",
                              100 * ( 1 / p -> composite_attack_speed() - 1 ), p -> stats.haste_rating, 100 * ( 1 / p -> buffed_attack_speed - 1 ) );
  print_xml_player_attribute( writer, "armor",
                              p -> composite_armor(), ( p -> stats.armor + p -> stats.bonus_armor ), p -> buffed_armor );
  print_xml_player_attribute( writer, "miss",
                              100 * ( p -> composite_tank_miss( SCHOOL_PHYSICAL ) ), 0, 100 * p -> buffed_miss );
  print_xml_player_attribute( writer, "dodge",
                              100 * ( p -> composite_tank_dodge() - p -> diminished_dodge() ), p -> stats.dodge_rating, 100 * p -> buffed_dodge );
  print_xml_player_attribute( writer, "parry",
                              100 * ( p -> composite_tank_parry() - p -> diminished_parry() ), p -> stats.parry_rating, 100 * p -> buffed_parry );
  print_xml_player_attribute( writer, "block",
                              100 * p -> composite_tank_block(), p -> stats.block_rating, 100 * p -> buffed_block );
  print_xml_player_attribute( writer, "tank_crit",
                              100 * p -> composite_tank_crit( SCHOOL_PHYSICAL ), 0, 100 * p -> buffed_crit );

  writer.begin_tag( "resource" );
  writer.print_attribute( "name", "health" );
  writer.print_attribute( "base", util_t::to_string( p -> resource_max[ RESOURCE_HEALTH ], 0 ) );
  writer.print_attribute( "buffed", util_t::to_string( p -> resource_buffed[ RESOURCE_HEALTH ], 0 ) );
  writer.end_tag(); // </resource>

  writer.begin_tag( "resource" );
  writer.print_attribute( "name", "mana" );
  writer.print_attribute( "base", util_t::to_string( p -> resource_max[ RESOURCE_MANA ], 0 ) );
  writer.print_attribute( "buffed", util_t::to_string( p -> resource_buffed[ RESOURCE_MANA ], 0 ) );
  writer.end_tag(); // </resource>
}

static void print_xml_player_attribute( xml_writer_t & writer, const std::string & attribute, double initial, double gear, double buffed )
{
  writer.begin_tag( "attribute" );
  writer.print_attribute( "name", attribute );
  writer.print_attribute( "base", util_t::to_string( initial, 0 ) );
  writer.print_attribute( "gear", util_t::to_string( gear, 0 ) );
  writer.print_attribute( "buffed", util_t::to_string( buffed, 0 ) );
  writer.end_tag(); // </attribute>
}

static void print_xml_player_actions( xml_writer_t & writer, player_t* p )
{
  writer.begin_tag( "glyphs" );
  std::vector<std::string> glyph_names;
  int num_glyphs = util_t::string_split( glyph_names, p -> glyphs_str.c_str(), "/" );
  for ( int i=0; i < num_glyphs; i++ )
  {
    writer.begin_tag( "glyph" );
    writer.print_attribute( "name", glyph_names[i] );
    writer.end_tag(); // </glyph>
  }
  writer.end_tag(); // </glyphs>

  writer.begin_tag( "priorities" );
  std::vector<std::string> action_list;
  int num_actions = util_t::string_split( action_list, p -> action_list_str, "/" );
  for ( int i=0; i < num_actions; i++ )
  {
    writer.begin_tag( "action" );
    writer.print_attribute( "index", util_t::to_string( i ) );
    writer.print_attribute( "value", action_list[ i ] );
    writer.end_tag(); // </action>
  }
  writer.end_tag(); // </priorities>


  writer.begin_tag( "actions" );

  for ( stats_t* s = p -> stats_list; s; s = s -> next )
  {
    if ( s -> num_executes > 1 || s -> compound_amount > 0 )
    {
      writer.begin_tag( "action" );
      writer.print_attribute( "name", s -> name_str );
      writer.print_attribute( "count", util_t::to_string( s -> num_executes, 1 ) );
      writer.print_attribute( "frequency", util_t::to_string( s -> frequency, 2 ) );
      writer.print_attribute( "dpe", util_t::to_string( s -> ape, 0 ) );
      writer.print_attribute( "dpe_percent", util_t::to_string( s -> portion_amount * 100.0, 0 ) );
      writer.print_attribute( "dpet", util_t::to_string( s -> apet, 0 ) );
      writer.print_attribute( "apr", util_t::to_string( s -> apr, 1 ) );
      writer.print_attribute( "pdps", util_t::to_string( s -> portion_aps.mean, 0 ) );
      if( s -> num_direct_results > 0 )
      {
        writer.begin_tag( "miss" );
        writer.print_attribute( "pct", util_t::to_string( s -> direct_results[ RESULT_MISS ].pct, 1 ) );
        writer.end_tag();
      }
      if( s -> direct_results[ RESULT_HIT ].avg_amount > 0 )
      {
        writer.begin_tag( "hit" );
        writer.print_attribute( "avg", util_t::to_string( s -> direct_results[ RESULT_HIT ].avg_amount, 0 ) );
        writer.print_attribute( "min", util_t::to_string( s -> direct_results[ RESULT_HIT ].min_amount, 0 ) );
        writer.print_attribute( "max", util_t::to_string( s -> direct_results[ RESULT_HIT ].max_amount, 0 ) );
        writer.end_tag(); // </hit>
      }
      if ( s -> direct_results[ RESULT_CRIT ].avg_amount > 0 )
      {
        writer.begin_tag( "crit" );
        writer.print_attribute( "avg", util_t::to_string( s -> direct_results[ RESULT_CRIT ].avg_amount, 0 ) );
        writer.print_attribute( "min", util_t::to_string( s -> direct_results[ RESULT_CRIT ].min_amount, 0 ) );
        writer.print_attribute( "max", util_t::to_string( s -> direct_results[ RESULT_CRIT ].max_amount, 0 ) );
        writer.print_attribute( "pct", util_t::to_string( s -> direct_results[ RESULT_CRIT ].pct, 1 ) );
        writer.end_tag(); // </crit>
      }
      if ( s -> direct_results[ RESULT_GLANCE ].avg_amount > 0 )
      {
        writer.begin_tag( "glance" );
        writer.print_attribute( "avg", util_t::to_string( s -> direct_results[ RESULT_GLANCE ].avg_amount, 0 ) );
        writer.print_attribute( "pct", util_t::to_string( s -> direct_results[ RESULT_GLANCE ].pct, 1 ) );
        writer.end_tag(); // </glance>
      }
      if ( s -> direct_results[ RESULT_DODGE ].count > 0 )
      {
        writer.begin_tag( "dodge" );
        writer.print_attribute( "pct", util_t::to_string( s -> direct_results[ RESULT_DODGE ].pct, 1 ) );
        writer.end_tag();
      }
      if ( s -> direct_results[ RESULT_PARRY ].count > 0 )
      {
        writer.begin_tag( "parry" );
        writer.print_attribute( "pct", util_t::to_string( s -> direct_results[ RESULT_PARRY ].pct, 1 ) );
        writer.end_tag();
      }
      if ( s -> num_ticks > 0 )
      {
        writer.begin_tag( "tick" );
        writer.print_attribute( "count", util_t::to_string( s -> num_ticks ) );
        writer.end_tag();
      }
      if ( s -> tick_results[ RESULT_HIT ].avg_amount > 0 || s -> tick_results[ RESULT_CRIT ].avg_amount > 0 )
      {
        writer.begin_tag( "miss_tick" );
        writer.print_attribute( "pct", util_t::to_string( s -> tick_results[ RESULT_MISS ].pct, 1 ) );
        writer.end_tag();
      }
      if ( s -> tick_results[ RESULT_HIT ].avg_amount > 0 )
      {
        writer.begin_tag( "tick" );
        writer.print_attribute( "avg", util_t::to_string( s -> tick_results[ RESULT_HIT ].avg_amount, 0 ) );
        writer.print_attribute( "min", util_t::to_string( s -> tick_results[ RESULT_HIT ].min_amount, 0 ) );
        writer.print_attribute( "max", util_t::to_string( s -> tick_results[ RESULT_HIT ].max_amount, 0 ) );
        writer.end_tag();
      }
      if ( s -> tick_results[ RESULT_CRIT ].avg_amount > 0 )
      {
        writer.begin_tag( "crit_tick" );
        writer.print_attribute( "avg", util_t::to_string( s -> tick_results[ RESULT_CRIT ].avg_amount, 0 ) );
        writer.print_attribute( "min", util_t::to_string( s -> tick_results[ RESULT_CRIT ].min_amount, 0 ) );
        writer.print_attribute( "max", util_t::to_string( s -> tick_results[ RESULT_CRIT ].max_amount, 0 ) );
        writer.print_attribute( "pct", util_t::to_string( s -> tick_results[ RESULT_CRIT ].pct, 1 ) );
        writer.end_tag();
      }
      if ( s -> total_tick_time > 0 )
      {
        writer.begin_tag( "uptime" );
        writer.print_attribute( "pct", util_t::to_string( 100.0 * s -> total_tick_time / s -> player -> fight_length.mean, 1 ) );
        writer.end_tag();
      }

      writer.end_tag(); // </action>6
    }
  }


  writer.end_tag(); // </actions>
}

static void print_xml_player_buffs( xml_writer_t & writer, player_t * p )
{
  writer.begin_tag( "buffs" );

  for ( buff_t* b = p -> buff_list; b; b = b -> next )
  {
    if ( b -> quiet || ! b -> start_count )
      continue;

    writer.begin_tag( "buff" );
    writer.print_attribute( "name", b -> name() );
    writer.print_attribute( "type", b -> constant ? "constant" : "dynamic" );

    if ( b -> constant )
    {
      writer.print_attribute( "start", util_t::to_string( b -> avg_start, 1 ) );
      writer.print_attribute( "refresh", util_t::to_string( b -> avg_refresh, 1 ) );
      writer.print_attribute( "interval", util_t::to_string( b -> avg_start_interval, 1 ) );
      writer.print_attribute( "trigger", util_t::to_string( b -> avg_trigger_interval, 1 ) );
      writer.print_attribute( "uptime", util_t::to_string( b -> uptime_pct, 0 ) );

      if( b -> benefit_pct > 0 && b -> benefit_pct < 100 )
      {
        writer.print_attribute( "benefit", util_t::to_string( b -> benefit_pct ) );
      }
    }
    writer.end_tag(); // </buff>
  }

  writer.end_tag(); // </buffs>
}

static void print_xml_player_uptime( xml_writer_t & writer, player_t * p )
{
  writer.begin_tag( "benefits" );

  for ( benefit_t* u = p -> benefit_list; u; u = u -> next )
  {
    if ( u -> ratio > 0 )
    {
      writer.begin_tag( "benefit" );
      writer.print_attribute( "name", u -> name() );
      writer.print_attribute( "ratio_pct", util_t::to_string( u -> ratio * 100.0, 1 ) );
      writer.end_tag();
    }
  }

  writer.end_tag(); // </benefits>


  writer.begin_tag( "uptimes" );

  for ( uptime_t* u = p -> uptime_list; u; u = u -> next )
  {
    if ( u -> uptime > 0 )
    {
      writer.begin_tag( "uptime" );
      writer.print_attribute( "name", u -> name() );
      writer.print_attribute( "pct", util_t::to_string( u -> uptime * 100.0, 1 ) );
      writer.end_tag();
    }
  }

  writer.end_tag(); // </uptimes>
}

static void print_xml_player_procs( xml_writer_t & writer, player_t * p )
{
  writer.begin_tag( "procs" );

  for ( proc_t* proc = p -> proc_list; proc; proc = proc -> next )
  {
    if ( proc -> count > 0 )
    {
      writer.begin_tag( "proc" );
      writer.print_attribute( "name", proc -> name() );
      writer.print_attribute( "count", util_t::to_string( proc -> count, 1 ) );
      writer.print_attribute( "frequency", util_t::to_string( proc -> frequency, 2 ) );
      writer.end_tag(); // </proc>
    }
  }

  writer.end_tag(); // </procs>
}

static void print_xml_player_gains( xml_writer_t & writer, player_t * p )
{
  writer.begin_tag( "gains" );

  for ( gain_t* g = p -> gain_list; g; g = g -> next )
  {
    if ( g -> actual > 0 || g -> overflow > 0 )
    {
      writer.begin_tag( "gain" );
      writer.print_attribute( "name", g -> name() );
      writer.print_attribute( "actual", util_t::to_string( g -> actual, 1 ) );
      double overflow_pct = 100.0 * g -> overflow / ( g -> actual + g -> overflow );
      if ( overflow_pct > 1.0 )
        writer.print_attribute( "overflow_pct", util_t::to_string( overflow_pct, 1 ) );
      writer.end_tag();
    }
  }

  writer.end_tag();
}

static void print_xml_player_scale_factors( xml_writer_t & writer, player_t * p )
{
  if ( ! p -> sim -> scaling -> has_scale_factors() ) return;

  if ( p -> is_add() || p -> is_enemy() ) return;

  if ( p -> sim -> report_precision < 0 )
    p -> sim -> report_precision = 2;

  writer.begin_tag( "scale_factors" );

  gear_stats_t& sf = p -> scaling;
  gear_stats_t& sf_norm = p -> scaling_normalized;

  writer.begin_tag( "weights" );

  for ( int i=0; i < STAT_MAX; i++ )
  {
    if ( p -> scales_with[ i ] != 0 )
    {
      writer.begin_tag( "stat" );
      writer.print_attribute( "name", util_t::stat_type_abbrev( i ) );
      writer.print_attribute( "value", util_t::to_string( sf.get_stat( i ), p -> sim -> report_precision ) );
      writer.print_attribute( "normalized", util_t::to_string( sf_norm.get_stat( i ), p -> sim -> report_precision ) );
      writer.print_attribute( "scaling_error", util_t::to_string( p -> scaling_error.get_stat( i ), p -> sim -> report_precision ) );
      writer.print_attribute( "delta", util_t::to_string( p -> sim -> scaling -> stats.get_stat( i ) ) );

      writer.end_tag(); // </stat>
    }
  }

  size_t num_scaling_stats = p -> scaling_stats.size();
  for ( size_t i=0; i < num_scaling_stats; i++ )
  {
    writer.begin_tag( "scaling_stat" );
    writer.print_attribute( "name", util_t::stat_type_abbrev( p -> scaling_stats[ i ] ) );
    writer.print_attribute( "index", util_t::to_string( ( int64_t )i ) );

    if ( i > 0 )
    {
      if ( ( ( p -> scaling.get_stat( p -> scaling_stats[ i - 1 ] ) - p -> scaling.get_stat( p -> scaling_stats[ i ] ) )
             > sqrt ( p -> scaling_compare_error.get_stat( p -> scaling_stats[ i - 1 ] ) * p -> scaling_compare_error.get_stat( p -> scaling_stats[ i - 1 ] ) / 4 + p -> scaling_compare_error.get_stat( p -> scaling_stats[ i ] ) * p -> scaling_compare_error.get_stat( p -> scaling_stats[ i ] ) / 4 ) * 2 ) )
        writer.print_attribute( "relative_to_previous", ">" );
      else
        writer.print_attribute( "relative_to_previous", "=" );
    }

    writer.end_tag(); // </scaling_stat>
  }

  writer.end_tag(); // </weights>

  if ( p -> sim -> scaling -> normalize_scale_factors )
  {
    writer.begin_tag( "dps_per_point" );
    writer.print_attribute( "stat", util_t::stat_type_abbrev( p -> normalize_by() ) );
    writer.print_attribute( "value", util_t::to_string( p -> scaling.get_stat( p -> normalize_by() ), p -> sim -> report_precision ) );
    writer.end_tag(); // </dps_per_point>
  }
  if ( p -> sim -> scaling -> scale_lag )
  {
    writer.begin_tag( "scale_lag_ms" );
    writer.print_attribute( "value", util_t::to_string( p -> scaling_lag, p -> sim -> report_precision ) );
    writer.print_attribute( "error", util_t::to_string( p -> scaling_lag_error, p -> sim -> report_precision ) );
    writer.end_tag(); // </scale_lag_ms>
  }


  std::string lootrank   = p -> gear_weights_lootrank_link;
  std::string wowhead    = p -> gear_weights_wowhead_link;
  std::string wowreforge = p -> gear_weights_wowreforge_link;
  //std::string pawn_std   = p -> gear_weights_pawn_std_string;
  //std::string pawn_alt   = p -> gear_weights_pawn_alt_string;

  writer.begin_tag( "link" );
  writer.print_attribute( "name", "wowhead" );
  writer.print_attribute( "type", "ranking" );
  writer.print_attribute_unescaped( "href", wowhead );
  writer.end_tag(); // </link>

  writer.begin_tag( "link" );
  writer.print_attribute( "name", "lootrank" );
  writer.print_attribute( "type", "ranking" );
  writer.print_attribute( "href", lootrank );
  writer.end_tag(); // </link>

  writer.begin_tag( "link" );
  writer.print_attribute( "name", "wowreforge" );
  writer.print_attribute( "type", "optimizer" );
  writer.print_attribute( "href", wowreforge );
  writer.end_tag(); // </link>

  writer.end_tag(); // </scale_factors>
}

static void print_xml_player_dps_plots( xml_writer_t & writer, player_t * p )
{
  sim_t* sim = p -> sim;

  if ( sim -> plot -> dps_plot_stat_str.empty() ) return;

  int range = sim -> plot -> dps_plot_points / 2;

  double min = -range * sim -> plot -> dps_plot_step;
  double max = +range * sim -> plot -> dps_plot_step;

  int points = 1 + range * 2;

  writer.begin_tag( "dps_plot_data" );
  writer.print_attribute( "min", util_t::to_string( min, 1 ) );
  writer.print_attribute( "max", util_t::to_string( max, 1 ) );
  writer.print_attribute( "points", util_t::to_string( points ) );

  for( int i=0; i < STAT_MAX; i++ )
  {
    std::vector<double>& pd = p -> dps_plot_data[ i ];

    if ( ! pd.empty() )
    {
      writer.begin_tag( "dps" );
      writer.print_attribute( "stat", util_t::stat_type_abbrev( i ) );
      size_t num_points = pd.size();
      for( size_t j=0; j < num_points; j++ )
      {
        writer.print_tag( "value", util_t::to_string( pd[ j ], 0 ) );
      }
      writer.end_tag(); // </dps>
    }
  }

  writer.end_tag(); // </dps_plot_data>
}

static void print_xml_player_charts( xml_writer_t & writer, player_t * p )
{
  writer.begin_tag( "charts" );

  if( ! p -> action_dpet_chart.empty() )
  {
    writer.begin_tag( "chart" );
    writer.print_attribute( "type", "dpet" );
    writer.print_attribute_unescaped( "href", p -> action_dmg_chart );
    writer.end_tag(); // </chart>
  }

  if( ! p -> action_dmg_chart.empty() )
  {
    writer.begin_tag( "chart" );
    writer.print_attribute( "type", "dmg" );
    writer.print_attribute_unescaped( "href", p -> action_dmg_chart );
    writer.end_tag(); // </chart>
  }

  if( ! p -> scaling_dps_chart.empty() )
  {
    writer.begin_tag( "chart" );
    writer.print_attribute( "type", "scaling_dps" );
    writer.print_attribute_unescaped( "href", p -> scaling_dps_chart );
    writer.end_tag(); // </chart>
  }

  if( ! p -> reforge_dps_chart.empty() )
  {
    writer.begin_tag( "chart" );
    writer.print_attribute( "type", "reforge_dps" );
    writer.print_attribute_unescaped( "href", p -> reforge_dps_chart );
    writer.end_tag(); // </chart>
  }

  if( ! p -> scale_factors_chart.empty() )
  {
    writer.begin_tag( "chart" );
    writer.print_attribute( "type", "scale_factors" );
    writer.print_attribute_unescaped( "href", p -> scale_factors_chart );
    writer.end_tag(); // </chart>
  }

  if( ! p -> timeline_dps_chart.empty() )
  {
    writer.begin_tag( "chart" );
    writer.print_attribute( "type", "timeline_dps" );
    writer.print_attribute_unescaped( "href", p -> timeline_dps_chart );
    writer.end_tag(); // </chart>
  }

  if( ! p -> distribution_dps_chart.empty() )
  {
    writer.begin_tag( "chart" );
    writer.print_attribute( "type", "distribution_dps" );
    writer.print_attribute_unescaped( "href", p -> distribution_dps_chart );
    writer.end_tag(); // </chart>
  }

  if( ! p -> time_spent_chart.empty() )
  {
    writer.begin_tag( "chart" );
    writer.print_attribute( "type", "time_spent" );
    writer.print_attribute_unescaped( "href", p -> time_spent_chart );
    writer.end_tag(); // </chart>
  }

  writer.end_tag(); // </charts>
}

static void print_xml_buffs( sim_t* sim, xml_writer_t & writer )
{
  writer.begin_tag( "buffs" );

  for ( buff_t* b = sim -> buff_list; b; b = b -> next )
  {
    if ( b -> quiet || ! b -> start_count )
      continue;

    writer.begin_tag( "buff" );
    writer.print_attribute( "name", b -> name() );
    writer.print_attribute( "type", b -> constant ? "constant" : "dynamic" );

    if ( b -> constant )
    {
      writer.print_attribute( "start", util_t::to_string( b -> avg_start, 1 ) );
      writer.print_attribute( "refresh", util_t::to_string( b -> avg_refresh, 1 ) );
      writer.print_attribute( "interval", util_t::to_string( b -> avg_start_interval, 1 ) );
      writer.print_attribute( "trigger", util_t::to_string( b -> avg_trigger_interval, 1 ) );
      writer.print_attribute( "uptime", util_t::to_string( b -> uptime_pct, 0 ) );

      if( b -> benefit_pct > 0 && b -> benefit_pct < 100 )
      {
        writer.print_attribute( "benefit", util_t::to_string( b -> benefit_pct ) );
      }

      if( b -> trigger_pct > 0 && b -> trigger_pct < 100 )
      {
        writer.print_attribute( "trigger", util_t::to_string( b -> trigger_pct ) );
      }
    }
    writer.end_tag(); // </buff>
  }

  writer.end_tag(); // </buffs>
}

struct compare_hat_donor_interval
{
  bool operator()( player_t* l, player_t* r ) SC_CONST
  {
    return( l -> procs.hat_donor -> frequency < r -> procs.hat_donor -> frequency );
  }
};

static void print_xml_hat_donors( sim_t* sim, xml_writer_t & writer )
{
  std::vector<player_t*> hat_donors;

  int num_players = ( int ) sim -> players_by_name.size();
  for ( int i=0; i < num_players; i++ )
  {
    player_t* p = sim -> players_by_name[ i ];
    if( p -> procs.hat_donor -> count )
      hat_donors.push_back( p );
  }

  int num_donors = ( int ) hat_donors.size();
  if( num_donors )
  {
    std::sort( hat_donors.begin(), hat_donors.end(), compare_hat_donor_interval()  );

    writer.begin_tag( "honor_among_thieves" );

    for( int i=0; i < num_donors; i++ )
    {
      writer.begin_tag( "donors" );
      player_t* p = hat_donors[ i ];
      proc_t* proc = p -> procs.hat_donor;
      writer.print_attribute( "name", p -> name() );
      writer.print_attribute( "frequency_sec", util_t::to_string( proc -> frequency, 2 ) );
      writer.print_attribute( "frequency_pct", util_t::to_string( ( 1.0 / proc -> frequency ), 3 ) );
      writer.end_tag(); // </donors>
    }

    writer.end_tag(); // </honor_among_thieves>
  }
}

static void print_xml_performance( sim_t* sim, xml_writer_t & writer )
{
  writer.begin_tag( "performance" );

  writer.print_tag( "total_events", util_t::to_string( sim -> total_events_processed ) );
  writer.print_tag( "max_event_queue", util_t::to_string( sim -> max_events_remaining ) );
  writer.print_tag( "target_health", util_t::to_string( sim -> target -> resource_base[ RESOURCE_HEALTH ], 0 ) );
  writer.print_tag( "sim_seconds", util_t::to_string( sim -> iterations * sim -> simulation_length.mean, 0 ) );
  writer.print_tag( "cpu_seconds", util_t::to_string( sim -> elapsed_cpu_seconds, 3 ) );
  writer.print_tag( "speed_up", util_t::to_string( sim -> iterations * sim -> simulation_length.mean / sim -> elapsed_cpu_seconds, 0 ) );
  writer.begin_tag( "rng" );
  writer.print_attribute( "roll", util_t::to_string( ( sim -> rng -> expected_roll  == 0 ) ? 1.0 : ( sim -> rng -> actual_roll  / sim -> rng -> expected_roll  ), 6 ) );
  writer.print_attribute( "range", util_t::to_string( ( sim -> rng -> expected_range == 0 ) ? 1.0 : ( sim -> rng -> actual_range / sim -> rng -> expected_range ), 6 ) );
  writer.print_attribute( "gauss", util_t::to_string( ( sim -> rng -> expected_gauss == 0 ) ? 1.0 : ( sim -> rng -> actual_gauss / sim -> rng -> expected_gauss ), 6 ) );
  writer.end_tag(); // </rng>

  writer.end_tag(); // </performance>
}

static void print_xml_config( sim_t* sim, xml_writer_t & writer )
{
  writer.begin_tag( "config" );
  time_t rawtime;
  time ( &rawtime );

  writer.print_tag( "timestamp", ctime( &rawtime ) );
  writer.print_tag( "iterations", util_t::to_string( sim -> iterations ) );

  double min_length = sim -> max_time * ( 1 - sim -> vary_combat_length );
  double max_length = sim -> max_time * ( 1 + sim -> vary_combat_length );
  writer.begin_tag( "fight_length" );
  if ( sim -> vary_combat_length > 0.0 )
    writer.print_attribute( "min", util_t::to_string( min_length, 0 ) );
  writer.print_attribute( "max", util_t::to_string( max_length, 0 ) );
  writer.end_tag(); // </fight_length>

  writer.print_tag( "fight_style", sim -> fight_style );
  writer.print_tag( "has_scale_factors", sim -> scaling -> has_scale_factors() ? "true" : "false" );
  writer.print_tag( "report_pets_separately", sim -> report_pets_separately ? "true" : "false" );
  writer.print_tag( "normalize_scale_factors", sim -> scaling ->normalize_scale_factors ? "true" : "false" );

  writer.end_tag(); // </config>
}

static void print_xml_summary( sim_t* sim, xml_writer_t & writer )
{
  writer.begin_tag( "summary" );
  writer.begin_tag( "dmg" );
  writer.print_attribute( "total", util_t::to_string( sim -> total_dmg, 0 ) );
  writer.print_attribute( "dps", util_t::to_string( sim -> raid_dps, 0 ) );
  writer.end_tag(); // </dmg>
  if( sim -> total_heal > 0 )
  {
    writer.begin_tag( "heal" );
    writer.print_attribute( "total", util_t::to_string( sim -> total_heal, 0 ) );
    writer.print_attribute( "hps", util_t::to_string( sim -> raid_hps, 0 ) );
    writer.end_tag(); // </heal>
  }

  int count = ( int ) sim -> dps_charts.size();
  writer.begin_tag( "charts" );
  for ( int i=0; i < count; i++ )
  {
    writer.begin_tag( "chart" );
    writer.print_attribute( "type", "dps" );
    writer.print_attribute_unescaped( "img_src", sim -> dps_charts[ i ] );
    writer.end_tag(); // </chart>
  }
  count = ( int ) sim -> gear_charts.size();
  for ( int i=0; i < count; i++ )
  {
    writer.begin_tag( "chart" );
    writer.print_attribute( "type", "gear" );
    writer.print_attribute_unescaped( "img_src", sim -> gear_charts[ i ] );
    writer.end_tag(); // </chart>
  }
  count = ( int ) sim -> dpet_charts.size();
  for ( int i=0; i < count; i++ )
  {
    writer.begin_tag( "chart" );
    writer.print_attribute( "type", "dpet" );
    writer.print_attribute_unescaped( "img_src", sim -> dpet_charts[ i ] );
    writer.end_tag(); // </chart>
  }
  writer.end_tag();

  print_xml_raid_events( sim, writer );

  writer.end_tag(); // </summary>
}

} // ANONYMOUS NAMESPACE ====================================================


// report_t::print_xml ======================================================

void report_t::print_xml( sim_t* sim )
{
  int num_players = ( int ) sim -> players_by_name.size();

  if ( num_players == 0 ) return;
  if ( sim -> simulation_length.mean == 0 ) return;
  if ( sim -> xml_file_str.empty() ) return;

  xml_writer_t writer( sim -> xml_file_str.c_str() );
  if( !writer.ready() )
  {
    sim -> errorf( "Unable to open xml file '%s'\n", sim -> xml_file_str.c_str() );
    return;
  }

  writer.init_document( sim -> xml_stylesheet_file_str );
  writer.begin_tag( "simulationcraft" );

  writer.print_attribute( "major_version", SC_MAJOR_VERSION );
  writer.print_attribute( "minor_version", SC_MINOR_VERSION );
  writer.print_attribute( "wow_version", dbc_t::wow_version( sim -> dbc.ptr ) );
  writer.print_attribute( "ptr", sim -> dbc.ptr ? "true" : "false" );
  writer.print_attribute( "wow_build", dbc_t::build_level( sim -> dbc.ptr ) );

#if SC_BETA

  writer.print_attribute( "beta", "true" );

#endif

  print_xml_config( sim, writer );
  print_xml_summary( sim, writer );

  print_xml_raid_events( sim, writer );
  print_xml_roster( sim, writer );
  print_xml_targets( sim, writer );

  print_xml_buffs( sim, writer );
  print_xml_hat_donors( sim, writer );
  print_xml_performance( sim, writer );

  print_xml_errors( sim, writer );

  writer.end_tag(); // </simulationcraft>
}

