import json
import os

file_json = os.path.join(os.path.dirname(__file__), "group_data.json")
file_txt = os.path.join(os.path.dirname(__file__), "group_data.csv")

with open(file_json, 'r', encoding='utf-8') as f:
    data_list = json.load(f)

with open(file_txt, 'r', encoding='utf-8') as j:
    data_1 = j.read()

da_el=[]
data=data_1.split("\n")
da=len(data)
data_siz=0
for i in range(da):
    le_da = len(data[i].split(","))
    if le_da == 12:
        data_siz+=1
    elif le_da == 1:
        if data[i].split(",")[0] == "":
            if da==1:
                print("データがありません")
            else:
                print(f"{i+1}行目が空白行です")
        else:
            print(f"{i+1}行目にデータが1つしかありません")
    elif le_da < 12:
        print(f"{i+1}行目にデータが{len(data[i].split(','))}つしかありません")
    elif le_da > 12:
        print(f"{i+1}行目にデータが{len(data[i].split(','))}つあります")

if da == data_siz:
    for i in range(data_siz):
        asd = data[i].split(",")

        asdfg = {
            "id": asd[6],
            "floor": asd[7]+"F",
            "roomName": asd[8],
            "groupName": asd[9],
            "title": asd[10],
            "desc": asd[11],
            "mainMap": {"x": asd[0],"y": asd[1]},
            "modalArea": {"left": asd[2],"top": asd[3],"width": asd[4],"height": asd[5]}
        }

        for j, item in enumerate(data_list):
            if item["id"] == asdfg["id"]:
                data_list[j] = asdfg
                break
        else:
            data_list.append(asdfg)

    with open(file_json, 'w', encoding='utf-8') as f:
        json.dump(data_list, f, indent=4, ensure_ascii=False)

    print("データが更新されました")