from selenium import webdriver
from selenium.webdriver.firefox.options import Options
import time
import sys

def exit_program(num=0, driver=None):
    if (driver):
        driver.close()
    sys.exit(num)

def login(driver, url, login, password):
    try:
        driver.get(url)
    except Exception as e:
        print(str(e))
        exit_program(1, driver)
    try:
        driver.find_element_by_name('email').send_keys(login)
        driver.find_element_by_name('passwd').send_keys(password)
        driver.find_element_by_name('login').click()
    except Exception as e:
        print('Failed to log in')
        exit_program(1, driver)
    if (driver.current_url =='https://reg.optimizator.ru/login.php'):
        print('Failed to log in')
        exit_program(1, driver)
    return driver
    
def get_cadic_info(driver):
    info = {}
    p = []
    post = {}
    passport = {}
    line = []
    cadic = "http://cadic.name/whoisgen/whoisgen.xml.php?n=1"
    try:
        driver.get(cadic)
    except Exception as e:
        print("An error occured while trying to access cadic")
        print(str(e))
        exit_program()
    lname, fname, ot = driver.find_element_by_tag_name("rus_name").text.split()
    info = {'lname':lname, 'fname':fname, 'ot':ot}
    info['lname'] = lname
    info['fname'] = fname
    info['ot'] = ot
    lname, ot, fname = driver.find_element_by_tag_name("eng_name").text.split()
    info['eng_lname'] = lname
    info['eng_fname'] = fname
    info['eng_ot'] = ot
    line = driver.find_element_by_tag_name("passport").text.split()
    passport['num']=line[0]
    passport['g_b']=(line[2]+' '+line[3]+' '+line[4]+' '+line[5])
    d,m,y=line[6].split('.')
    passport['date']=[d,m,y]
    info['pass']=passport
    line=driver.find_element_by_tag_name("post_address").text.split(',')
    post['index']=line[0].strip()
    post['city']=line[1].strip()
    post['addr']=(line[2]+' '+line[3]+' '+line[4]).strip()
    post['reciever']=line[5].strip()
    info['post'] = post
    elem = driver.find_element_by_tag_name("phone").text
    info['phone'] = elem
    d,m,y=driver.find_element_by_tag_name("birthdate").text.split('.')
    if (int(info['pass']['date'][2])-int(y) < 21):
        y=str(int(y)-1)
    p.append(d)
    p.append(m)
    p.append(y)
    info['birth'] = p
    return info

def profile_reg(driver, info, url, profname):
    name=[
        'profile_name','person_r_lname', 'person_r_fname',
        'person_r_ot','birth_date_day',
        'birth_date_month','birth_date_year','person_lname',
        'person_fname','person_ot','passport_num','passport_kem_vidan',
        'passport_day','passport_month','passport_year',
        'p_addr_zip','p_addr_area','p_addr_city','p_addr_addr',
        'p_addr_recipient','phone', 'email'
    ]
    r=[
        profname, info['lname'], info['fname'], info['ot'],info['birth'][0],
        info['birth'][1], info['birth'][2], info['eng_lname'],
        info['eng_fname'], info['eng_ot'], info['pass']['num'],
        info['pass']['g_b'], info['pass']['date'][0], info['pass']['date'][1],
        info['pass']['date'][2], info['post']['index'], info['post']['city'],
        info['post']['city'], info['post']['addr'],
        info['post']['reciever'], info['phone'], info['email']
    ]
    driver.get(url)
    driver.find_element_by_name('profile_name').clear()
    for i in range(0, len(name)):
        try:
            driver.find_element_by_name(name[i]).clear()
        except:
            pass
        driver.find_element_by_name(name[i]).send_keys(r[i])
    driver.find_element_by_id('birth_date_day').send_keys('29')
    driver.find_element_by_name('save_profile').click()
    if ('profiles.php' in driver.current_url):
        print('Failed to create profile. Trying again with another data')
        return -1
    return 0

def domen_reg(driver, url, domain, dns, profname):
    driver.get(url)
    driver.find_element_by_tag_name('textarea').send_keys(domain)
    driver.find_element_by_id('send_to_basket').click()
    try:
        driver.find_element_by_id('send_for_reg_2').click()
    except:
        print("This domain name is already taken or you don't have enough money for registration")
        return -1
    driver.find_element_by_id('autorenew_off').click()
    for i in range(0, len(dns)):
        driver.find_element_by_name('ns{}'.format(i)).clear()
        driver.find_element_by_name('ns{}'.format(i)).send_keys(dns[i])
    driver.find_element_by_name('r_person_id').send_keys(profname)
    driver.find_element_by_id('send_for_reg').click()
    try:
        elem = driver.find_element_by_class_name("b-reg_finished__status")
        print(elem.text)
    except:
        pass

def return_lines(input_file, lines):
    with open(input_file, 'w') as f:
        for line in lines:
            f.write(line)
    
def read_line(input_file, driver):
    try:
        with open(input_file, 'r') as f:
            args = f.readline().split(':')
            print(args)
            lines = f.readlines()
    except:
        print('Error while reading file')
        exit_program(1, driver)
    if (len(args)<2):
        exit_program()
    if (len(args) < 7):
        print('Not enough input data')    
        exit_program(0, driver)
    return args, lines
    

reg_opt = "https://reg.optimizator.ru/login.php"
reg_opt_logout = "https://reg.optimizator.ru/logout.php"
reg_opt_profile = "https://reg.optimizator.ru/profiles/profiles.php?type=f"
reg_opt_reg = "https://reg.optimizator.ru/d_reg/"
atname = "https://atname.ru/reg/login.php"
atname_logout="https://atname.ru/reg/logout.php"
atname_profile = "https://atname.ru/reg/profiles/profiles.php?type=f"
atname_reg='https://atname.ru/reg/d_reg'
args = []
try:
    input_file = sys.argv[1]
except:
    print('Specify an input file')
    exit_program(0)
options = Options()
options.headless = False
driver = webdriver.Firefox(options=options)

while(1):
    dns = []
    args, lines = read_line(input_file, driver)
    try:
        for i in range(6, 10):
            dns.append(args[i])
    except:
        pass
    info = get_cadic_info(driver)
    info['email']=args[5]
    print(info)
    if ('atname' in args[0]):
        print('Logging in atname.ru')
        login(driver, atname, args[1], args[2])
        print('Logged in successfully')
        print('Creating profile {}'.format(args[4]))
        res = profile_reg(driver, info, atname_profile, args[4])
        time.sleep(10)
        if (res == -1):
            driver.get(atname_logout)
            continue
        print('Profile is created')
        print('Registrating domain {}'.format(args[3]))
        res = domen_reg(driver, atname_reg, args[3], dns, args[4])
        time.sleep(10)
        if (res != -1):
            print("Domain {} is successfully registered".format(args[3]))
        driver.get(atname_logout)
        time.sleep(10)
    elif ('reg.optimizator' in args[0]):
        print('logging in reg.optimizator.ru')
        login(driver, reg_opt, args[1], args[2])
        print('Logged in successfully')
        print('Creating profile {}'.format(args[4]))
        res = profile_reg(driver, info, reg_opt_profile, args[4])
        time.sleep(10)
        if (res == -1):
            continue
        print('Profile created')
        print('Registrating domain {}'.format(args[3]))
        res=domen_reg(driver, reg_opt_reg, args[3], dns, args[4])
        time.sleep(10)
        if (res != -1):
            print("Domain {} is successfully registered".format(args[3]))
        driver.get(reg_opt_logout)
        time.sleep(10)
    else:
        print('Error: wrong registration website')
    return_lines(input_file, lines)
driver.close()
