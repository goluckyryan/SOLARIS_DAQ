#!/bin/bash

#url=http://localhost:3000/dashboard/snapshot/qHdJjri9Uk7DPbwnFh0fIBDEELMn4nzP
#url="http://localhost:3000/d/iXH6cG-Vz/test-dashboard?orgId=1&refresh=5s&from=1678824051707&to=1678827651708"

#firefox -screenshot http://solarisdaq:3000/dashboard/snapshot/qHdJjri9Uk7DPbwnFh0fIBDEELMn4nzP haha.png

#cutycapt --url=${url} --out=haha.png

#wget --output-document="haha.png" ${url}

usr=GeneralSOLARIS
pwd='gam$hippie'

APIKEY=eyJrIjoid25Bc3U3NEdxVDZYV2xrdEJ0QTlxNTdFM25JcXVTTlAiLCJuIjoiYWRtaW5LRVkiLCJpZCI6MX0=

url=http://localhost:3000/

#curl http://${usr}:${pwd}@localhost:3000/api/search
#curl http://${usr}:${pwd}@localhost:3000/api/admin/settings

#curl -v -H "Accept: application/json" -H "Content-Type: application/json"  http://${usr}:${pwd}@localhost:3000/api/dashboard/snapshots

#get list of snapshot
#curl -v  http://${usr}:${pwd}@localhost:3000/api/dashboard/snapshots

#delet snapshot
#curl -X DELETE http://${usr}:${pwd}@localhost:3000/api/snapshots/HzQd70cK1NhqZ7lDV7bSU4GkmNMezacs

#curl -v -X -d ${msg} http://${usr}:${pwd}@localhost:3000/api/snapshots 

#curl -X DELETE -H "Authorization: Bearer ${APIKEY}" ${url}api/snapshots/Yn7MnDIzXsOL3VoRS617N17K5ojTsnOk

# curl -X POST -H "Authorization: Bearer ${APIKEY}" -H "Accept: application/json" -H "Content-Type: application/json" -d '{
#       "dashboard": {
#         "editable":false,
#         "nav":[{"enable":false, "type":"timepicker"}],
#         "rows": [ {} ],
#         "style":"dark",
#         "tags":[],
#         "templating":{ "list":[]},
#         "time":{ },
#         "timezone":"browser",
#         "title":"Home",
#         "version":5
#       },
#       "expires": 2
#     }' ${url}/api/snapshots


#curl -v -X GET -H "Authorization: Bearer ${APIKEY}" -H "Accept: application/json" ${url}api/dashboard/snapshots

#curl -v -X GET -H "Authorization: Bearer ${APIKEY}" -H "Accept: application/json" ${url}api/search

#successfull single panel
#curl 'http://localhost:3000/render/d-solo/iXH6cG-Vz/test-dashboard?orgId=1&refresh=5s&from=now-6h&to=now&panelId=2&width=1000&height=500&tz=America%2FNew_York' -H "Authorization: Bearer ${APIKEY}" --compressed > haha.png


curl 'http://localhost:3000/render/d-solo/iXH6cG-Vz/test-dashboard?orgId=1&refresh=5s&from=now-6h&to=now&panelId=2' -H "Authorization: Bearer ${APIKEY}" --compressed > haha.png

eog haha.png