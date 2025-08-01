# xSTUDIO Security

While we strive to ensure that xSTUDIO itself doesn't include any security vulnerabilities the product does of course have a number of external dependencies that themselves may be vulnerable to exploitation. As such users are recommended to always be vigilant about any software installations that might compromise their system's security.

Note that xSTUDIO runs an embedded Python interpreter on startup that imports system installed Python modules. xSTUDIO also scans the 'XSTUDIO_PYTHON_PLUGIN_PATH' environment variable at runtime for dynamic loading of Python plugins which are instanced when the application starts up. Please excercise appropriate caution and only ever install xSTUDIO plugins or Python utilities from trusted sources.

## Reporting Vulnerabilities

Quickly resolving security related issues is a priority. 
To report a security issue, please use the GitHub Security Advisory ["Report a Vulnerability"](https://github.com/AcademySoftwareFoundation/xstudio/security/advisories/new) tab.

Include detailed steps to reproduce the issue, and any other information that
could aid an investigation. Someone will assess the report and make every
effort to respond within 14 days.

## Outstanding Security Issues

None

## Addressed Security Issues

None