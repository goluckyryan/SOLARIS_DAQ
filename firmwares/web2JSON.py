import requests
from bs4 import BeautifulSoup

url = "http://192.168.0.102/documentation/a00103.html"
response = requests.get(url)
soup = BeautifulSoup(response.content, 'html.parser')

print(url)

# print(soup)

# Get the entire div
project_div = soup.find('div', id='projectname')
print("Full Text:", project_div.get_text(strip=True))

# Get just the main project name (excluding the span)
project_name = project_div.contents[0].strip()
print("Name:", project_name)

# Get the project number from the span
project_number = project_div.find('span', id='projectnumber').get_text(strip=True)
print("Number:", project_number)

safe_name = project_name.replace(" ", "_")
filename = f"{safe_name}_{project_number}_parameters.json"

import json

data = []

# Iterate through all <h2> blocks which denote parameters
for h2 in soup.find_all("h2"):
    param = {}
    name = h2.get_text(strip=True)
    param['name'] = name

    # Description is usually the next <p>
    desc_tag = h2.find_next_sibling('p')
    if desc_tag:
        param['description'] = desc_tag.get_text(strip=True)

    # Get the next two blockquotes: one for options, one for allowed values
    blockquotes = desc_tag.find_all_next('blockquote', class_='doxtable', limit=2)
    
    # Parse Options
    if len(blockquotes) > 0:
        options = {}
        for li in blockquotes[0].find_all('li'):
            text = li.get_text(strip=True)
            if ':' in text:
                key, val = text.split(':', 1)
                options[key.strip()] = val.strip()
        param['options'] = options

    # Parse Allowed Values
    if len(blockquotes) > 1:
        allowed_values = []
        for li in blockquotes[1].find_all('li'):
            b_tag = li.find('b')
            em_tag = li.find('em')
            if b_tag and em_tag:
                allowed_values.append({
                    'value': b_tag.get_text(strip=True),
                    'description': em_tag.get_text(strip=True)
                })
        param['allowed_values'] = allowed_values

    data.append(param)


# print(parameters)


# Output as JSON
with open(filename, 'w') as f:
    json.dump(data, f, indent=2)