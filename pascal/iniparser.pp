{ iniparser.pp -- an ini file parser from my C redo of Chloe Kudryavtsev's code }

{ original article https://pencil.toast.cafe/bunker-labs/simply-parse-in-c }
{ from 2023, site will not load (SSL) in October 2025 so I grabbed a snap  }
{ from the wayback machine and it is in this repository for reference.     }

{ Chloe Kudryavtsev had no license listed; see README and LICENSE in this  }
{ repository for legalese, but I consider anything I write as being in the }
{ Public Domain.                                                           }

{ I'm doing this to play with callbacks in Pascal. To be useful this would }
{ be put in a unit but I don't know that I will do that.                   }

{ My normal FPC settings. The only one that might confuse people is that I }
{ do not short-circuit boolean expressions.                                }

{$MODE OBJFPC}
{$LONGSTRINGS ON}
{$IOCHECKS ON}
{$RANGECHECKS ON}
{$BOOLEVAL ON}
{$GOTO ON}

{ Specification:                                                       }
{                                                                      }
{ Rules of ini:                                                        }
{                                                                      }
{ There is no standard definition, but ini files fall under the "I     }
{ know it when I see it" rubric. This is a parser and it cares not one }
{ bit about semantics.                                                 }
{                                                                      }
{ There are three possible expressions in an ini file definition:      }
{                                                                      }
{ - comment line          # this is a comment ; so is this             }
{ - section header        [some text]                                  }
{ - key value pair        key = value                                  }
{                                                                      }
{ Blank lines are not significant. an expression may not span          }
{ multiple lines. Leading and trailing whitespace are ignored.         }
{                                                                      }
{ A null key in a key = value expression is an error "= something\n".  }
{                                                                      }
{ A null value in a key = value epression is allowed "key =\n".        }
{                                                                      }
{ The closing square brace on a section header may be omitted.         }

program iniparser;

uses 

   Classes, SysUtils, StrUtils, StreamEx, BufStream;

const

   { useful character constants }
   TAB           = chr(09);
   NL            = chr(10);
   SPACE         = ' ';
   LEFT_BRACKET  = '[';
   RIGHT_BRACKET = ']';
   EQUAL         = '=';
   HASH          = '#';
   SEMICOLON     = ';';

   { sets for TrimXset to chop noise }
   { off the end of a line.          }
   DEPAD            = [ SPACE, TAB ];
   TO_START_SECTION = [ LEFT_BRACKET, SPACE ];
   FROM_END_SECTION = [ RIGHT_BRACKET, SPACE ];
   START_COMMENT    = [ HASH, SEMICOLON ];

   { string versions of the above characters and    }
   { sets for the library functions that need them. }
   SPLIT_KEY_VALUE  = EQUAL;

type

   tStatus = (sError, sEOF, sOK);

{ Callback function for the parser. As each completed key:value       }
{ is parsed, post the data back to the parser client. The parsed      }
{ values are passed by reference but should not be modified.          }
{                                                                     }
{ var   : section, a string [section] from an ini file                }
{ var   : key, a string       key = value from an ini file            }
{ var   : value, a string                                             }
{ in/out: user_data, a pointer sized area that can be used            }
{         to store or reference context for the parser client         }
{ return: a boolean, "should parser terminate?"                       }

type
   TIniCallback = function(
      var section : string;
      var key     : string;
      var value   : string;
      userData    : Pointer
      ): boolean;

{ read next expression                                                }
{                                                                     }
{ skip over intervening whitespace until an expression start is       }
{ recognized.                                                         }
{                                                                     }
{ the expressions:                                                    }
{                                                                     }
{ [section name]                                                      }
{ ; comment                                                           }
{ # another comment                                                   }
{ key = value                                                         }
{                                                                     }
{    var: a properly positioned stream reader                         }
{    var: string for section                                          }
{    var: string for key                                              }
{    var: string for value                                            }
{    var: status (ok, error, eof)                                     }
{ return: success or error                                            }
{                                                                     }
{ as each value is collected, invoke the callback function            }
{ to give the client the current section, key, and value.             }

function getnext( var ini     : TStreamReader;
                  var section : string;
                  var key     : string;
                  var value   : string;
                  var status  : tStatus): boolean;

var
   line : string;
   i, p : integer;
   kv   : array of string; { dynamic array }

begin
   result := true;
   status := sOK;

while true do
   begin

      { trailing check }
      if ini.EOF then
         begin
            status := sEOF;
            break;
         end;

      { get next line, filtering blank and comments }
      line := ini.ReadLine;
      if line = '' then
         continue;
      line := TrimSet(line, DEPAD);
      if line = '' then
         continue;
      if line[1] in START_COMMENT then
         continue;

      { check for error, <nil> = ... }
      if line[1] = EQUAL then
         begin
            status := sError;
            break;
         end;

      { section header? }
      if line[1] = LEFT_BRACKET then    
         begin
            line := TrimLeftSet(line, TO_START_SECTION);
            line := TrimRightSet(line, FROM_END_SECTION);
            section := line;
            key := '';
            value := '';
            status := sOK;
            break;
         end;
         
      { We must find an = }
      p := Pos(EQUAL, line);
      if p = 0 then
         begin
            status := sError; { illegal expression no = }   
            break;
         end;

      { extract key and value ... value may be empty }
      kv := SplitString(line, EQUAL);
      key := '';
      value := '';
      case length(kv) of
         1: key := TrimSet(kv[0], DEPAD);
         2: begin
               key := TrimSet(kv[0], DEPAD);
               value := TrimSet(kv[1], DEPAD);
            end;
         else { 0 is not possible due to split due to Pos }
            begin
               key := TrimSet(kv[0],DEPAD);
               value := TrimLeftSet(kv[1], DEPAD);
               for i := 2 to length(kv)-1 do
                  value := value + EQUAL + kv[i];
            end;
      end;
      break;
   end; { while true }
   result := status = sOK;
end; { function readNext }

{ ParseIni                                                            }
{                                                                     }
{ Given a TStreamReader holding the contents of an ini file, parse    }
{ its contents.                                                       }
{                                                                     }
{ An ini file is made up of comments (# or ;), sections ([section     }
{ name]), and key:value pairs (key = value).                          }
{                                                                     }
{ As each key:value pair is completed, post the section, key,         }
{ and value to a client provided callback.                            }
{                                                                     }
{ var   : file stream on an ini file                                  }
{ const : expected to be a pointer, or any pointer sized              }
{         object holding any state needed by the parser               }
{         client.                                                     }
{ const : function pointer of the callback function.                  }
{ return: boolean success or failure.                                 }

function ParseIni(
            var   iniFile      : TStreamReader;
            const userData     : Pointer;
            const callback     : TIniCallback
            ): boolean;

var
   status: tStatus;
   section, key, value: string;
   
begin
   section := '';
   key := '';
   value := '';
   iniFile.Reset;
   while getNext(iniFile, section, key, value, status) do
      begin
         { if key is an empty string then we just read a }
         { section header. We don't invoke the callback  }
         { until a key:value pair is read.               }
         if key = '' then
            continue;

         { *** post to our client via the callback *** }
         { at this point we have a completed key:value }
         { pair, pass the section, key, and value back }
         { to the client.                              }
         {                                             }
         { the client returns true if the parse should }
         { terminate early.                            }
         if callback(section, key, value, userdata) then
            break;
         key := '';
         value := '';
      end;
   result := status <> sError;
end;

(* *********************************** *)
{ simple test harness for an ini file parser. }

{ a callback function for the parser. this just prints the ini file as }
{ interpreted.                                                         }

// type
//    TIniCallback = function(
//       var section : string;
//       var key     : string;
//       var value   : string;
//       userData    : Pointer
//       ): boolean;
function cbIniParser(var section: string;
                     var key: string;
                     var value: string;
                     ctx: Pointer): boolean;
var
   lastSection: string = ''; { static }
begin
   if lastSection <> section then
      write(nl, 'Section:', section, NL);
   lastSection := section;
   write('key:value      ', key, ':', value, NL);
   result := (section = 'STOP') and (key = 'STOP') and (value = 'STOP');
end;

var
   ini: TStreamReader;
   parseStatus: boolean;
   cb: TIniCallback;

{ the actual test driver. }
begin
   cb := @cbIniParser;
   writeln('Testing iniparser: ');
   if paramCount < 1 then
      begin
         writeln(stderr, 'error no file name given');
         halt; 
      end;

   try
      ini := TStreamReader.Create(TBufferedFileStream.Create(paramStr(1), fmOpenRead));
   except
      on E: Exception do
         begin
            writeln(stderr, 'Error opening ini file ', paramStr(1));
            writeln(stderr, '>>> ', E.ClassName, ' - ', E.Message);
            halt;
         end;
   end; { try }

   try
      parseStatus := ParseIni(ini, nil, cb);
      writeln(NL, 'parse complete, returned ', parseStatus);
      if not parseStatus then
         begin
            writeln(stderr, 'Error failure in parse of ', paramStr(1));
            writeln(stderr, 'Review input file');
            halt;
         end;
   except      
      on E: Exception do
         begin
            writeln(stderr, 'Error: ', E.ClassName, ' - ', E.Message);
            halt;
         end;
   end; { try }

   ini.Free;
   
end.
