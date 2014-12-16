/*NEXUS IRC session BNC, by Subsentient. This software is public domain.*/

/**This file is where most of the big stuff is called from. main() is also in here.**/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "netcore.h"
#include "nexus.h"
#include "config.h"
#include "irc.h"
#include "server.h"
#include "state.h"

int main(int argc, char **argv)
{ ///And as I once cleansed the world with fire, I will destroy you, and your puny project! -Dr. Reed
	//Print version and whatnot.
	
	//Turn off buffering. We hates it! -Gollum
	setvbuf(stdout, NULL, _IONBF, 0); //Really, all buffering serves to do for us is mess up our console output. No thanks.
	setvbuf(stderr, NULL, _IONBF, 0);
	
	puts("NEXUS BNC " NEXUS_VERSION "\n");
	
	///Process command line arguments.
	if (argc > 1)
	{ //Command line arguments override any values in configuration files.
		unsigned Inc = 1;
		const char *ArgData = NULL;
		
		for (; Inc < argc; ++Inc)
		{
			if (!strcmp("--help", argv[Inc]))
			{
				printf("Available options:\n\n"
						"--ircserver=myserver.com\n"
						"--ircport=6667\n"
						"--ircnick=mynickname\n"
						"--ircident=myidentname\n"
						"--ircrealname=myrealname\n"
						"--ircnickservuser=username\n"
						"--ircnickservpassword=password\n"
						"--maxsimulclient=1024\n"
						"--nexusport=6667\n");
				exit(0);
			}
			else if (!strncmp(argv[Inc], "--ircserver=", sizeof "--ircserver" - 1))
			{ //IRC server hostname.
				ArgData = argv[Inc] + sizeof "--ircserver=" - 1;
				if (!*ArgData) continue;

				strncpy(IRCConfig.Server, ArgData, sizeof IRCConfig.Server - 1);
				IRCConfig.Server[sizeof IRCConfig.Server - 1] = '\0';
			}
			else if (!strncmp(argv[Inc], "--ircport=", sizeof "--ircport=" - 1))
			{ //IRC port number.
				ArgData = argv[Inc] + sizeof "--ircport=" - 1;
				if (!*ArgData) continue;

				IRCConfig.PortNum = atoi(ArgData);
			}
			else if (!strncmp(argv[Inc], "--ircnick=", sizeof "--ircnick=" - 1))
			{
				ArgData = argv[Inc] + sizeof "--ircnick=" - 1;
				if (!*ArgData) continue;

				strncpy(IRCConfig.Nick, ArgData, sizeof IRCConfig.Nick - 1);
				IRCConfig.Nick[sizeof IRCConfig.Nick - 1] = '\0';
			}
			else if (!strncmp(argv[Inc], "--ircident=", sizeof "--ircident=" - 1))
			{
				ArgData = argv[Inc] + sizeof "--ircident=" - 1;
				if (!*ArgData) continue;

				strncpy(IRCConfig.Ident, ArgData, sizeof IRCConfig.Ident - 1);
				IRCConfig.Ident[sizeof IRCConfig.Ident - 1] = '\0';
			}
			else if (!strncmp(argv[Inc], "--ircrealname=", sizeof "--ircrealname=" - 1))
			{
				ArgData = argv[Inc] + sizeof "--ircrealname=" - 1;
				if (!*ArgData) continue;

				strncpy(IRCConfig.RealName, ArgData, sizeof IRCConfig.RealName - 1);
				IRCConfig.RealName[sizeof IRCConfig.RealName - 1] = '\0';
			}
			else if (!strncmp(argv[Inc], "--ircnickservuser=", sizeof "--ircnickservuser=" - 1))
			{
				ArgData = argv[Inc] + sizeof "--ircnickservuser=" - 1;
				
				if (!*ArgData) continue;
				
				strncpy(IRCConfig.NickServUser, ArgData, sizeof IRCConfig.NickServUser - 1);
				IRCConfig.NickServUser[sizeof IRCConfig.NickServUser - 1] = '\0';
			}
			else if (!strncmp(argv[Inc], "--ircnickservpassword=", sizeof "--ircnickservpassword=" - 1))
			{
				ArgData = argv[Inc] + sizeof "--ircnickservpassword=" - 1;
				
				if (!*ArgData) continue;
				
				strncpy(IRCConfig.NickServPassword, ArgData, sizeof IRCConfig.NickServPassword - 1);
				IRCConfig.NickServPassword[sizeof IRCConfig.NickServPassword - 1] = '\0';
			}
			else if (!strncmp(argv[Inc], "--maxsimulclients=", sizeof "--maxsimulclients=" - 1))
			{
				ArgData = argv[Inc] + sizeof "--maxsimulclients=" - 1;
				if (!*ArgData) continue;

				NEXUSConfig.MaxSimulConnections = atoi(ArgData);
			}
			else if (!strncmp(argv[Inc], "--nexusport=", sizeof "--nexusport=" - 1))
			{
				ArgData = argv[Inc] + sizeof "--nexusport=" - 1;
				if (!*ArgData) continue;
				
				NEXUSConfig.PortNum = atoi(ArgData);
			}
			else
			{
				fprintf(stderr, "Bad comand line option \"%s\". See --help for a list of options.\n", argv[Inc]);
				exit(1);
			}
		}
	}
	
	if (!Config_ReadConfig())
	{ //We might still get what we need from CLI arguments.
		fprintf(stderr, "WARNING: Failed to load config file!\n");
	}
	
	if (!Config_CheckConfig())
	{ //Bad configuration.
		fprintf(stderr, "Configuration for NEXUS is invalid.\n"
				"Please check your configuration and/or command line arguments.\n");
		return 1;
	}
	
	//Connect to the REAL IRC server.
	printf("Connecting to IRC server \"%s:%hu\"... ", IRCConfig.Server, IRCConfig.PortNum);
	if (!IRC_Connect())
	{
		fprintf(stderr, "Unable to connect to IRC server!\n");
		return 1;
	}
	puts("Done.");
	
	
	//Bring up the NEXUS pseudo-IRC-server.
	printf("Bringing up NEXUS server on port %hu... ", NEXUSConfig.PortNum);
	if (!Net_InitServer(NEXUSConfig.PortNum))
	{
		fprintf(stderr, "Failed to bring up NEXUS server on port %hu.\n", NEXUSConfig.PortNum);
		return 1;
	}
	puts("Done.");
	
	while (1)
	{
		IRC_Loop();
		Server_Loop();
	}
	
	return 0;
}

/*Processes data from a client and forwards it to the IRC server.*/
void NEXUS_NEXUS2IRC(const char *Message, struct ClientList *const Client)
{
	const enum ServerMessageType MsgType = Server_GetMessageType(Message);
	
	switch (MsgType)
	{
		char OutBuf[2048];
		
		default: //Nothing we care about.
		{
		ForwardVerbatim:
			//Append \r\n and send to IRC server.
			snprintf(OutBuf, sizeof OutBuf, "%s\r\n", Message);
			Net_Write(IRCDescriptor, OutBuf, strlen(OutBuf));
			
			break;
		}
		case SERVERMSG_PRIVMSG:
		{ //Some clients don't group our own speech correctly for PMs, so do this for channels only.
			struct ClientList *Worker = ClientListCore;
			const char *Search = strstr(Message, "PRIVMSG ");
			
			if (!Search) return; //WTF?
			
			Search += sizeof "PRIVMSG " - 1;
			
			//Don't do this for PMs. Just Channels.
			if (*Search != '#') goto ForwardVerbatim;
			
			//We need to make all text reach the other client.
			for (; Worker; Worker = Worker->Next)
			{
				if (Worker == Client) continue; //Don't send to the one who just sent this.
				
				snprintf(OutBuf, sizeof OutBuf, ":%s!%s@%s %s\r\n", IRCConfig.Nick, Client->Ident, Client->IP, Message);
				Net_Write(Worker->Descriptor, OutBuf, strlen(OutBuf));
			}
			goto ForwardVerbatim;
		}
		case SERVERMSG_WHO:
		{ //Only allow one client to spam WHO.
			if (Client != ClientListCore) break;
			else goto ForwardVerbatim;
		}
		case SERVERMSG_QUIT:
		{
			Server_SendQuit(Client->Descriptor);
			close(Client->Descriptor);
			Server_ClientList_Del(Client->Descriptor);
			return;
		}
	}
}

/*Processes data from the real IRC server and then sends it to functions that reformat
 * said data so that clients connected to NEXUS IRC won't get confused.*/
void NEXUS_IRC2NEXUS(const char *Message)
{
	const enum IRCMessageType MsgType = IRC_GetMessageType(Message);
	
	switch (MsgType)
	{
		char OutBuf[2048];
		
		default: //Nothing we explicitly have to deal with
		{ //Append a \r\n and then send it to everyone.
		ForwardVerbatim:
			snprintf(OutBuf, sizeof OutBuf, "%s\r\n", Message);
			Server_ForwardToAll(OutBuf);
			
			break;
		}
		case IRCMSG_TOPIC: //Either the server sent us a topic or someone has changed it.
		{
			unsigned Inc = 0;
			char NewTopic[1024];
			char Channel[256], *Worker = strchr(Message, '#');
			struct ChannelList *ChannelStruct = NULL;
			
			if (!Worker) return; //Corrupt data.
			
			//Get the channel name.
			for (; Worker[Inc] != ' ' && Worker[Inc] != '\0' && Inc < sizeof Channel - 1; ++Inc)
			{
				Channel[Inc] = Worker[Inc];
			}
			Channel[Inc] = '\0';
			
			Worker += Inc; //Skip past the space.
			
			if (*Worker++ == '\0') return; //Corrupt data.
			
			if (*Worker == ':') ++Worker;
			
			//Get the new topic.
			strncpy(NewTopic, Worker, sizeof NewTopic - 1);
			NewTopic[sizeof NewTopic - 1] = '\0';
		
			//Lookup the channel.
			if ((ChannelStruct = State_LookupChannel(Channel)) == NULL) return; //We aren't in that channel. Why send us a topic?
		
			//Copy in the new topic.
			strncpy(ChannelStruct->Topic, NewTopic, sizeof ChannelStruct->Topic - 1);
			ChannelStruct->Topic[sizeof ChannelStruct->Topic - 1] = '\0';
			
			goto ForwardVerbatim;
		}
		case IRCMSG_TOPICORIGIN:
		{ //Where the topic came from.
			char Channel[256], Setter[256];
			char *Worker = strchr(Message, '#');
			unsigned WhenSet = 0; //When the topic was set.
			unsigned Inc = 0;
			struct ChannelList *ChannelStruct = NULL;
			
			//Get the channel.
			for (; *Worker != ' ' && *Worker != '\0' && Inc < sizeof Channel - 1; ++Inc, ++Worker)
			{
				Channel[Inc] = *Worker;
			}
			Channel[Inc] = '\0';
			
			if (*Worker++ == '\0') return; //Corrupt data.
			
			//Lookup the channel.
			if (!(ChannelStruct = State_LookupChannel(Channel))) return; //Not in that channel.
			
			//Get the setter of the topic.
			for (Inc = 0; *Worker != ' ' && *Worker != '\0' && Inc < sizeof Setter - 1; ++Inc, ++Worker)
			{
				Setter[Inc] = *Worker;
			}
			Setter[Inc] = '\0';
			
			if (*Worker++ == '\0') return; //Another corruption check.
			
			//Get when we set this topic.
			WhenSet = (unsigned)atoll(Worker);
			
			//Copy in the setter.
			strncpy(ChannelStruct->WhoSetTopic, Setter, sizeof ChannelStruct->WhoSetTopic - 1);
			ChannelStruct->WhoSetTopic[sizeof ChannelStruct->WhoSetTopic - 1] = '\0';
			
			//Copy in the time it was set.
			ChannelStruct->WhenSetTopic = WhenSet;
			
			//Now send it to everyone.
			goto ForwardVerbatim;
			break;
		}
		case IRCMSG_NAMES: //The server is replying to a names command.
		{
			unsigned Inc = 0;
			const char *Search = strchr(Message, '#');
			char NamesChannel[512];
			char NamesNick[64]; //Used by the do-while loop.
			struct ChannelList *ChannelStruct = NULL;
			
			//Copy in the channel for this.
			for (; Search[Inc] != ' ' && Search[Inc] != '\0' && Inc < sizeof NamesChannel - 1; ++Inc)
			{
				NamesChannel[Inc] = Search[Inc];
			}
			NamesChannel[Inc] = '\0';
			
			//Can't find the channel.
			if (!(ChannelStruct = State_LookupChannel(NamesChannel))) return;
			
			if (Search[Inc] == '\0') return; //Bad data. Failed.
			
			//Skip past all whitespace and the colon.
			Search += Inc;
			while (*Search == ' ' || *Search == ':') ++Search;
			
			//Process each nick in the names list.
			do
			{
				unsigned char Modes = 0; //Are they OP or something?
				
				while (*Search == ' ') ++Search; //Skip past starting whitespace if reentering loop.
				
				//They are an OP or something. Store their symbol.
				if (*Search == '!' || *Search == '@' || *Search == '#'
					|| *Search == '$' || *Search == '%' || *Search == '&'
					|| *Search == '*' || *Search == '+'
					|| *Search == '=' || *Search == '-' || *Search == '('
					|| *Search == ')' || *Search == '?' || *Search == '~')
				{
					Modes = State_UserModes_Get_Symbol2Mode(*Search++);
				}
				
				//Get the nickname for this name.
				for (Inc = 0; Search[Inc] != ' ' && Search[Inc] != '\0' && Inc < sizeof NamesNick - 1; ++Inc)
				{
					NamesNick[Inc] = Search[Inc];
				}
				NamesNick[Inc] = '\0';
				
				
				State_AddUserToChannel(NamesNick, Modes, ChannelStruct);
				
			} while ((Search = strchr(Search, ' ')));
			
			//Now send the message to the client.
			snprintf(OutBuf, sizeof OutBuf, "%s\r\n", Message);
			Server_ForwardToAll(OutBuf);
			break;
		}
		case IRCMSG_PART:
		case IRCMSG_JOIN:
		case IRCMSG_NICK:
		{ //We need to make sure the ident and IP match the client's
			char NickTest[64];
			unsigned Inc = 0;
			const char *Test = Message + 1; //Skip past the ':' at the start.
			struct ClientList *Worker = ClientListCore;
			
			for (; Test[Inc] != '!' && Test[Inc] != ' ' && Test[Inc] != '\0' && Inc < sizeof NickTest - 1; ++Inc)
			{ //Copy in the nick for this join/part message.
				NickTest[Inc] = Test[Inc];
			}
			NickTest[Inc] = '\0';
			
			if (!strcmp(NickTest, IRCConfig.Nick)) //This is our nick.
			{ //Since it's our nick, we gotta alter it so that the IP and ident match the client's.
				for (; Worker; Worker = Worker->Next)
				{ //Rebuild so it matches their ident and IP.
					if (IRC_AlterMessageOrigin(Message, OutBuf, sizeof OutBuf, Worker))
					{ //If we fail to alter the origin just don't send it.
						strncat(OutBuf, "\r\n", sizeof OutBuf - strlen(OutBuf) - 2);
						Net_Write(Worker->Descriptor, OutBuf, strlen(OutBuf));
					}
				}
				
				switch (MsgType)
				{ //Some specific stuff we need to do
					case IRCMSG_NICK:
					{ //if it's our nickname, we need to remember that it's been changed.
						char NewNick[2048];
						
						//update the nick.
						IRC_GetMessageData(Message, NewNick);
						strcpy(IRCConfig.Nick, *NewNick == ':' ? NewNick + 1 : NewNick);
						break;
					}
					case IRCMSG_JOIN:
					{ //We joined a channel.
						char NewChannel[2048];
						struct ChannelList *Chan;
						IRC_GetMessageData(Message, NewChannel);
						
						Chan = State_AddChannel(NewChannel);
						State_AddUserToChannel(IRCConfig.Nick, 0, Chan);
						break;
					}
					case IRCMSG_PART:
					{ //We left a channel.
						char GoneChannel[2048], *Search;
						
						IRC_GetMessageData(Message, GoneChannel);
						
						//Delete whatever part message they may have placed. We don't care.
						if ((Search = strchr(GoneChannel, ' '))) *Search = '\0';
						
						//Now delete this parted channel.
						State_DelChannel(GoneChannel);
						break;
					}
					default:
						break;
				}
			}
			
			//It's not us. Forward verbatim.
			snprintf(OutBuf, sizeof OutBuf, "%s\r\n", Message);
			
			for (; Worker; Worker = Worker->Next)
			{
				Net_Write(Worker->Descriptor, OutBuf, strlen(OutBuf));
			}
			
			
			switch (MsgType)
			{
				char Nick[64], Ident[64], Mask[256];
				
				case IRCMSG_NICK:
				{ //Change the nick for them in all channels they exist in.
					struct ChannelList *Worker = ChannelListCore;
					char NewNick_[64], *NewNick = NewNick_;
					
					//Get their old nick.
					IRC_BreakdownNick(Message, Nick, Ident, Mask);
					IRC_GetMessageData(Message, NewNick_);
					
					if (*NewNick == ':') ++NewNick;
					
					for (; Worker; Worker = Worker->Next)
					{
						struct UserList *User = State_GetUserInChannel(Nick, Worker);
						unsigned char Modes = 0;
						
						//Doesn't exist in that channel.
						if (!User) continue;
						
						Modes = User->Modes;
						
						State_DelUserFromChannel(Nick, Worker); //Delete the old nickname.
						State_AddUserToChannel(NewNick, Modes, Worker); //Create the new one.
					}
					break;
				}
				case IRCMSG_PART:
				case IRCMSG_JOIN:
				{ //Add this user to the channel.
					char DaChannel[512], *Search;
					struct ChannelList *ChannelStruct;
					
					IRC_BreakdownNick(Message, Nick, Ident, Mask);
					IRC_GetMessageData(Message, DaChannel);
					
					if ((Search = strchr(DaChannel, ' '))) *Search = '\0';
					
					if (!(ChannelStruct = State_LookupChannel(DaChannel))) break;

					if (MsgType == IRCMSG_JOIN)
					{
						State_AddUserToChannel(Nick, 0, ChannelStruct);
					}
					else
					{
						State_DelUserFromChannel(Nick, ChannelStruct);
					}
					break;
				}
				default:
					break;
			}
			break;
		}
		case IRCMSG_MODE:
		{
			const char *Worker = strchr(Message, '#');
			char Channel[256], Nick[64];
			struct UserList *UserStruct = NULL;
			struct ChannelList *ChannelStruct = NULL;
			unsigned Inc = 0;
			char ModeLetter = 0;
			bool SettingMode = false;
			
			if (!Worker) return; //Not a channel.
			
			
			//Get the channel the mode affects.
			for (; *Worker != ' ' && *Worker != '\0' && Inc < sizeof Channel - 1; ++Inc, ++Worker)
			{
				Channel[Inc] = *Worker;
			}
			Channel[Inc] = '\0';
			
			if (*Worker == '\0') return; //Corrupted.
			
			while (*Worker == ' ') ++Worker;
			
			switch (*Worker)
			{ //Determine if we are stopping or starting this mode.
				case '+':
					SettingMode = true;
					break;
				case '-':
					SettingMode = false;
					break;
				default:
					return; //Bad data.
			}
			
			++Worker; //Now on to the letter that tells us what mode.
			
			//Look up the mode by its letter.
			ModeLetter = State_UserModes_Get_Letter2Mode(*Worker);
			
			if (!ModeLetter) return; //Not something we can deal with.
			
			//Now get the nickname.
			if (!(Worker = strchr(Worker, ' '))) return; //Bad data again.
			
			//Skip past the spaces.
			while (*Worker == ' ') ++Worker;
			
			//Check if this is a mask. If so, return. We are only handling user modes here.
			if (strchr(Worker, '!')) return;
			
			//Now, actually get the nickname.
			for (Inc = 0; *Worker != '\0' && Inc < sizeof Nick - 1; ++Inc, ++Worker)
			{
				Nick[Inc] = *Worker;
			}
			Nick[Inc] = '\0';
			
			//Look up the channel.
			if (!(ChannelStruct = State_LookupChannel(Channel))) return; //Can't find the channel.
			
			//Look up this user.
			if (!(UserStruct = State_GetUserInChannel(Nick, ChannelStruct))) return; //Can't find the nick.
			
			//Now alter their mode accordingly.
			if (SettingMode)
			{
				UserStruct->Modes |= ModeLetter;
			}
			else
			{
				UserStruct->Modes &= ~ModeLetter;
			}
			goto ForwardVerbatim; //Now send the original message to the clients.
		}
		case IRCMSG_QUIT:
		{ //Handles quitting per-user.
			char Nick[64], Ident[64], Mask[256];
			IRC_BreakdownNick(Message, Nick, Ident, Mask);
			struct ChannelList *Worker = ChannelListCore;
			
			if (!strcmp(Nick, IRCConfig.Nick))
			{ //Uhoh, it's us.
				const char *QuitReason = strchr(Message + 1, ':'); //+ 1 to skip past the FIRST ':'.
				
				if (QuitReason)
				{
					snprintf(OutBuf, sizeof OutBuf,
							":" NEXUS_FAKEHOST " PRIVMSG %s :NEXUS was disconnected from the IRC server. "
							"The reason given was \"%s\". NEXUS is shutting down.\r\n", IRCConfig.Nick, QuitReason + 1);
				}
				else
				{
					snprintf(OutBuf, sizeof OutBuf,
							":" NEXUS_FAKEHOST " PRIVMSG %s :NEXUS was disconnected from the IRC server. "
							"No reason was provided by the server. NEXUS is shutting down.\r\n", IRCConfig.Nick);
				}

					
				Server_ForwardToAll(OutBuf);
				
				IRC_Disconnect(); //Close the IRC server connection.
				Server_SendQuit(-1); //Tell all clients to quit.
				Net_ShutdownServer(); //Bring down the NEXUS server.
				exit(1);
			}
			
			//Yay! Not us!
			for (; Worker; Worker = Worker->Next)
			{
				State_DelUserFromChannel(Nick, Worker); //If it doesn't find it, that's fine.
			}
			
			//Now forward to all our users.
			snprintf(OutBuf, sizeof OutBuf, "%s\r\n", Message);
			Server_ForwardToAll(OutBuf);
		}
	}
}


