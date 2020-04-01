// Macro to make quick template function to resolve A/W functions

#ifndef Kx_MakeWinUnicodeCallWrapper
	#ifdef UNICODE
		#define Kx_MakeWinUnicodeCallWrapper(funcName)	\
			template<class... Args>	\
			auto funcName(Args&&... arg)	\
			{	\
				return ::funcName##W(std::forward<Args>(arg)...);	\
			}
	#else
		#define Kx_MakeWinUnicodeCallWrapper(funcName)	\
					template<class... Args>	\
					auto funcName(Args&&... arg)	\
					{	\
						return ::funcName##A(std::forward<Args>(arg)...);	\
					}
	#endif
#endif

// Undefine Unicode wrapper macros
#undef min
#undef max
#undef GetObject
#undef SetCurrentDirectory
#undef CopyFile
#undef MoveFile
#undef DeleteFile
#undef GetBinaryType
#undef MessageBox
#undef GetFreeSpace
#undef PlaySound
#undef RegisterClass
#undef CreateEvent
#undef GetFirstChild
#undef GetNextSibling
#undef GetPrevSibling
#undef GetWindowStyle
#undef GetShortPathName
#undef GetLongPathName
#undef GetFullPathName
#undef GetFileAttributes
#undef EnumResourceTypes
#undef LoadImage
#undef UpdateResource
#undef BeginUpdateResource
#undef EndUpdateResource
#undef EnumResourceLanguages
#undef FormatMessage
#undef GetCommandLine
#undef CreateProcess
#undef EnumProcesses
#undef GetUserName
#undef FindFirstFile
#undef FindNextFile
#undef GetEnvironmentVariable
#undef SetEnvironmentVariable
#undef ExitWindows
#undef GetCompressedFileSize
#undef GetTempPath
#undef CreateDirectory