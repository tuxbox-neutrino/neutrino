
namespace httpd
{

const char *notPermittedDetail()
{
}

const char *authLevelName(AuthLevel a)
{
	switch (a)
	{
		case AuthLevel::Public: return "public";
		case AuthLevel::Read:   return "read";
		case AuthLevel::Write:  return "write";
		case AuthLevel::System: return "system";
	}
	return "system";
}

const char *sessionCookieName()
{
}

const char *csrfHeaderName()
{
}

const char *queryTokenName()
{
}

} // namespace httpd
