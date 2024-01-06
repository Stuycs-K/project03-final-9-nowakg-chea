#include "role.h"



char* intToTeam(int i) {
    if(i == T_TOWN) return "Town";
    if(i == T_MAFIA) return "Mafia";
    if(i == T_NEUTRAL) return "Neutral";
    return "Error: invalid team num";
}

char* intToRole(int role, int team) {
    if(team == T_TOWN) {
        if(role == R_VETERAN) return "Veteran";
        if(role == R_MEDIUM) return "Medium";
        if(role == R_RETROBUTIONIST) return "Retrobutionist";
        if(role == R_DOCTOR) return "Doctor";
        if(role == R_LOOKOUT) return "Lookout";
        if(role == R_SHERIFF) return "Sheriff";
        if(role == R_JAILOR) return "Jailor";
        if(role == R_VIGILANTE) return "Vigilante";
        if(role == R_MAYOR) return "Mayor";
    }
    if(team == T_MAFIA) {
        if(role == R_BLACKMAILER) return "Blackmailer";
        if(role == R_CONSIGLIERE) return "Consigliere";
        if(role == R_GODFATHER) return "Godfather";
        if(role == R_MAFIOSO) return "Mafioso";
    }
    if(team == T_NEUTRAL) {
        if(role == R_SERIALKILLER) return "Serial Killer";
        if(role == R_JESTER) return "Jester";
        if(role == R_EXECUTIONER) return "Executioner";
    }
    return "Error: invalid team or role num";
}
